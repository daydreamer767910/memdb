#include "memtable.hpp"
#include "util/util.hpp"

MemTable::MemTable(const std::string& tableName, const std::vector<Column>& columns)
    : name_(tableName), columns_(columns) {
    if (name_.empty() || columns_.empty()) {
        throw std::invalid_argument("Invalid table structure.");
    }
    for (const auto& column : columns_) {
        if (column.name.empty() || column.type.empty()) {
            throw std::invalid_argument("Each column must have a name and a type.");
        }

        // 如果列可以为空且没有默认值，抛出异常
        if (column.nullable && std::holds_alternative<std::monostate>(column.defaultValue)) {
            throw std::invalid_argument("Nullable column must have a default value: " + column.name);
        }
    }
}

bool MemTable::validateRow(const Row& row) {
    if (row.size() != columns_.size()) {
        throw std::invalid_argument("Row size does not match column count.");
    }
    for (size_t i = 0; i < columns_.size(); ++i) {
        const auto& column = columns_[i];
        if (row[i].index() == 0 && !column.nullable) {
            throw std::invalid_argument("Missing required field: " + column.name);
        }
        if (!isValidType(row[i], column.type)) {
            throw std::invalid_argument("Invalid type for field: " + column.name);
        }
    }
    return true;
}


bool MemTable::validatePrimaryKey(const Row& row) {
    size_t currentRowIdx = rows_.size();  // 获取当前行号
    for (size_t i = 0; i < columns_.size(); ++i) {
        const auto& column = columns_[i];
        if (column.primaryKey) {
            // 获取主键值，基于列索引
            const auto& field = row[i];

            // 检查主键值是否存在于索引中
            std::visit([&](const auto& value) {
                // 如果主键值已经存在于索引中，抛出异常
                if (primaryKeyIndex_.find(value) != primaryKeyIndex_.end()) {
                    throw std::invalid_argument("Primary key value already exists: " + column.name);
                }
                // 如果主键值没有重复，插入主键值到主键索引中
                primaryKeyIndex_[value] = currentRowIdx;  // 使用当前行号
            }, field);
        }
    }
    return true;
}



Row MemTable::processRowDefaults(const Row& row) const {
    Row newRow;
    size_t rowIndex = 0;  // 用来追踪当前 row 中的列位置

    // 遍历每个列，检查列是否在 row 中有对应的字段
    for (size_t i = 0; i < columns_.size(); ++i) {
        const auto& column = columns_[i];
        
        // 检查 row 中是否有对应的字段，如果 row 中的列数不足，就插入默认值
        if (rowIndex < row.size()) {
            const Field& field = row[rowIndex];
            if (isValidType(field, column.type)) {
                // 如果 row 中有值并且类型匹配，直接插入
                newRow.push_back(field);
            } else {
                // 如果类型不匹配，用默认值
                //newRow.push_back(getDefault(column.type));
                if (column.type == "date") {
                    newRow.push_back(getDefault(column.type));
                } else {
                    newRow.push_back(column.defaultValue);
                }
            }
            rowIndex++;
        } else {
            // 如果 row 中没有更多的列，插入该列的默认值
            //newRow.push_back(getDefault(column.type));
            if (column.type == "date") {
                newRow.push_back(getDefault(column.type));
            } else {
                newRow.push_back(column.defaultValue);
            }
        }
    }
    
    return newRow;
}

Row MemTable::jsonToRow(const json& jsonRow) {
    Row row(columns_.size());  // 初始化一个 Row，大小为 columns_ 的大小

    // 遍历 JSON 对象的所有字段
    for (const auto& [key, value] : jsonRow.items()) {
        // 找到对应的列
        auto columnIt = std::find_if(columns_.begin(), columns_.end(), [&key](const Column& col) {
            return col.name == key;
        });

        if (columnIt != columns_.end()) {
            size_t index = std::distance(columns_.begin(), columnIt);  // 获取列的索引
            row[index] = jsonToField(columnIt->type, value);  // 使用列类型转换字段
        }
    }

    return row;
}

std::vector<Row> MemTable::jsonToRows(const json& jsonRows) {
    std::vector<Row> rows;

    // 创建一个列名到列索引的映射
    std::unordered_map<std::string, size_t> columnNameToIndex;
    for (size_t i = 0; i < columns_.size(); ++i) {
        columnNameToIndex[columns_[i].name] = i;
    }

    for (const auto& jsonRow : jsonRows["rows"]) {
        Row row(columns_.size());  // 初始化一个 Row，大小为 columns_ 的大小

        for (const auto& [key, value] : jsonRow.items()) {
            // 查找列名对应的索引
            auto columnIt = columnNameToIndex.find(key);
            if (columnIt != columnNameToIndex.end()) {
                size_t index = columnIt->second;  // 获取列的索引
                row[index] = jsonToField(columns_[index].type, value);  // 使用索引填充行
            }
        }

        rows.push_back(row);
    }

    return rows;
}



int MemTable::insertRowsFromJson(const json& jsonRows) {
    std::unique_lock<std::shared_mutex> lock(mutex_); // 独占锁
    std::vector<size_t> newIndexes; // 记录需要更新索引的行
    int i = 0;
    for (const auto& jsonRow : jsonRows["rows"]) {
        Row row(columns_.size());  // 初始化一个 Row，大小为 columns_ 的大小
        for (const auto& [key, value] : jsonRow.items()) {
            auto columnIt = std::find_if(columns_.begin(), columns_.end(), [&key](const Column& col) {
                return col.name == key;
            });

            if (columnIt != columns_.end()) {
                size_t index = std::distance(columns_.begin(), columnIt);  // 获取列的索引
                row[index] = jsonToField(columnIt->type, value);  // 使用索引填充行
            }
        }
        Row newRow = processRowDefaults(row);
        if (validateRow(newRow) && validatePrimaryKey(newRow)) {
            rows_.push_back(newRow);
            newIndexes.push_back(rows_.size() - 1);
            i++;
            
        }
    }

    // 批量更新索引
    updateIndexesBatch(newIndexes);
    return i;
}

bool MemTable::insertRow(const Row& row) {
    std::unique_lock<std::shared_mutex> lock(mutex_); // 独占锁
    Row newRow = processRowDefaults(row);
    if (validateRow(newRow) && validatePrimaryKey(newRow)) {
        rows_.push_back(newRow);
        updateIndexes(newRow, rows_.size() - 1);

        return true;
    }
    return false;
}


bool MemTable::insertRows(const std::vector<Row>& newRows) {
    std::unique_lock<std::shared_mutex> lock(mutex_); // 独占锁
    std::vector<size_t> newIndexes; // 记录需要更新索引的行
    for (const auto& row : newRows) {
        Row newRow = processRowDefaults(row);
        if (validateRow(newRow) && validatePrimaryKey(newRow)) {
            rows_.push_back(newRow);
            newIndexes.push_back(rows_.size() - 1);

        } else {
            return false;
        }
    }
    // 批量更新索引
    updateIndexesBatch(newIndexes);
    return true;
}

std::vector<Row> MemTable::getRows() const{
    std::shared_lock<std::shared_mutex> lock(mutex_); // 共享锁
    return rows_;
}

size_t MemTable::getTotalRows() const{
    std::shared_lock<std::shared_mutex> lock(mutex_); // 共享锁
    return rows_.size();
}

void MemTable::showIndexs() {
    for (const auto& [colName, index] : indexes_) {
        // 打印索引更新信息
        std::cout << "index for column: " << colName << "\n";
        for (const auto& pair : index) {
            std::cout << "value: " << fieldToJson(pair.first) << ", index: ";
            for (const auto& idx : pair.second) {
                std::cout << idx << " ";
            }
            std::cout << std::endl;
        }
    }
}

void MemTable::updateIndexes(const Row& row, int rowIndex) {
    for (size_t i = 0; i < columns_.size(); ++i) {
        const auto& column = columns_[i];
        if (indexes_.find(column.name) != indexes_.end()) {
            auto& index = indexes_[column.name];
            // 获取当前列的值
            auto columnValue = row[i];

            // 更新索引
            index[columnValue].insert(rowIndex);

            // 打印索引更新信息
            std::cout << "Updated index for column: " << column.name << "\n";
            std::cout << "  Value: ";
            std::visit([](auto&& val) { std::cout << val; }, columnValue);
            std::cout << ", Row Index: " << rowIndex << "\n";
        }
    }
}

void MemTable::updateIndexesBatch(const std::vector<size_t>& rowIdxes) {
    for (auto index : rowIdxes) {
        updateIndexes(rows_[index], index);
    }
}

void MemTable::addIndex(const std::string& columnName) {
    std::unique_lock<std::shared_mutex> lock(mutex_); // 独占锁
    Index index;
    
    // Find the column index corresponding to the columnName
    auto columnIt = std::find_if(columns_.begin(), columns_.end(),
        [&columnName](const Column& col) {
            return col.name == columnName;
        });

    if (columnIt == columns_.end()) {
        throw std::invalid_argument("Column not found: " + columnName);
    }

    size_t columnIndex = std::distance(columns_.begin(), columnIt); // Get the index of the column
    
    for (size_t i = 0; i < rows_.size(); ++i) {
        const Field& fieldValue = rows_[i][columnIndex]; // Get the value from the row using the column index
        index[fieldValue].insert(i); // Update the index
    }

    indexes_[columnName] = index;
}

std::vector<Row> MemTable::queryByIndex(const std::string& columnName, const Field& value) const {
    std::shared_lock<std::shared_mutex> lock(mutex_); // 共享锁

    std::vector<Row> result;
    
    // 使用 find 查找，避免使用 operator[]，不会修改 indexes
    auto it = indexes_.find(columnName);
    if (it != indexes_.end()) {
        const auto& index = it->second;
        auto itValue = index.find(value);
        if (itValue != index.end()) {
            for (const auto& rowIndex : itValue->second) {
                result.push_back(rows_[rowIndex]);
            }
        }
    }
    return result;
}

std::optional<Row> MemTable::findRowByPrimaryKey(const Field& primaryKey) const {
    std::shared_lock<std::shared_mutex> lock(mutex_); // 共享锁

    // 查找主键值是否存在于索引中
    auto it = primaryKeyIndex_.find(primaryKey);
    if (it != primaryKeyIndex_.end()) {
        // 如果找到，返回对应行
        return rows_[it->second];
    }

    // 如果主键不存在，返回 std::nullopt
    return std::nullopt;
}

// 获取从第 n 行开始的 limit 个数据
std::vector<Row> MemTable::getWithLimitAndOffset(int limit, int offset) const {
    std::shared_lock<std::shared_mutex> lock(mutex_); // 共享锁

    std::vector<Row> result;
    
    // 如果 offset 超过表中的行数，直接返回空结果
    if (offset >= rows_.size()) {
        return result;
    }

    // 从 offset 行开始，最多获取 limit 行
    for (int i = offset; i < std::min(static_cast<int>(rows_.size()), offset + limit); ++i) {
        result.push_back(rows_[i]);
    }

    return result;
}

size_t MemTable::getColumnIndex(const std::string& columnName) const {
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name == columnName) {
            return i;
        }
    }
    throw std::invalid_argument("Column not found: " + columnName);
}

std::vector<Row> MemTable::query(
    const std::string& columnName,
    const std::function<bool(const Field&)>& predicate) const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<Row> results;

    size_t colIdx = getColumnIndex(columnName);

    // 1. 主键查询优化（仅当 predicate 是等值查询时）
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name == columnName && columns_[i].primaryKey) {
            for (const auto& [key, idx] : primaryKeyIndex_) {
                if (predicate(key)) {
                    results.push_back(rows_[idx]);
                }
            }
            return results; // 主键查询结果直接返回
        }
    }

    // 2. 索引查询优化
    auto indexIt = indexes_.find(columnName);
    if (indexIt != indexes_.end()) {
        const auto& index = indexIt->second;
        for (const auto& [key, idxSet] : index) {
            // 如果 key 满足条件，遍历 set 中的所有 idx
            if (predicate(key)) {
                for (int idx : idxSet) {
                    results.push_back(rows_[idx]);
                }
            }
        }
        return results;
    }

    // 3. 全表遍历
    for (const auto& row : rows_) {
        if (predicate(row[colIdx])) {
            results.push_back(row);
        }
    }

    return results;
}

std::vector<Row> MemTable::query(
    const std::string& columnName,
    const std::string& op,        // 比较操作符，如 "==", "<", ">", 等
    const Field& queryValue) const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<Row> results;

    // 获取列索引
    size_t colIdx = getColumnIndex(columnName);

    // 1. 检查是否是主键列
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name == columnName && columns_[i].primaryKey) {
            // 主键查询，使用主键索引
            auto& index = primaryKeyIndex_;
            for (const auto& [key, idx] : index) {
                // 使用 std::visit 解包 variant
                bool match = std::visit([&](const auto& value) {
                    return compare(value, queryValue, op);
                }, key);

                if (match) {
                    // 主键是唯一的，直接返回对应的行
                    results.push_back(rows_[idx]);
                }
            }
            return results;
        }
    }

    // 2. 检查其他索引
    auto indexIt = indexes_.find(columnName);
    if (indexIt != indexes_.end()) {
        const auto& index = indexIt->second;
        for (const auto& [key, idxSet] : index) {
            // 使用 std::visit 解包 variant
            bool match = std::visit([&](const auto& value) {
                return compare(value, queryValue, op);
            }, key);

            if (match) {
                for (int idx : idxSet) {
                    results.push_back(rows_[idx]);
                }
            }
        }
        return results;
    }

    // 3. 全表扫描，遍历所有行
    for (const auto& row : rows_) {
        const auto& field = row[colIdx];

        // 使用 std::visit 来根据 Field 类型检查条件
        std::visit([&](const auto& value) {
            if (compare(value, queryValue, op)) {
                results.push_back(row);
            }
        }, field);
    }

    return results;
}

bool MemTable::update(
    const std::string& columnName,         // 待更新的列
    const Field& newValue,                 // 新值
    const std::string& op,                 // 比较操作符，如 "==", "<", ">", 等
    const Field& queryValue)              // 查询条件的值
{
    std::unique_lock<std::shared_mutex> lock(mutex_);  // 使用写锁，确保线程安全
    bool updated = false;

    // 获取列索引
    size_t colIdx = getColumnIndex(columnName);

    // 1. 检查是否是主键列
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name == columnName && columns_[i].primaryKey) {
            // 主键查询，使用主键索引
            auto& index = primaryKeyIndex_;
            for (auto& [key, idx] : index) {
                bool match = std::visit([&](const auto& value) {
                    return compare(value, queryValue, op);
                }, key);

                if (match) {
                    // 更新主键对应的行
                    rows_[idx][colIdx] = newValue;
                    // 更新主键索引（移除旧的主键，插入新的主键）
                    index.erase(key);
                    std::visit([&](const auto& value) {
                        index[value] = idx;  // 更新主键索引
                    }, newValue);
                    updated = true;
                }
            }
            return updated;
        }
    }

    // 2. 检查其他索引
    auto indexIt = indexes_.find(columnName);
    if (indexIt != indexes_.end()) {
        auto& index = indexIt->second;
        for (auto& [key, idxSet] : index) {
            bool match = std::visit([&](const auto& value) {
                return compare(value, queryValue, op);
            }, key);

            if (match) {
                for (int idx : idxSet) {
                    // 更新符合条件的行
                    rows_[idx][colIdx] = newValue;
                    // 更新索引：移除旧的值并插入新的值
                    index.erase(key); // 移除旧的索引值
                    std::visit([&](const auto& value) {
                        index[value].insert(idx);  // 插入新的索引值
                    }, newValue);
                    updated = true;
                }
            }
        }
        return updated;
    }

    // 3. 全表扫描，遍历所有行
    for (size_t i = 0; i < rows_.size(); ++i) {
        const auto& field = rows_[i][colIdx];

        // 使用 std::visit 来根据 Field 类型检查条件
        std::visit([&](const auto& value) {
            if (compare(value, queryValue, op)) {
                // 更新行
                rows_[i][colIdx] = newValue;
                updated = true;

                // 如果更新了索引列，需要同步更新索引
                if (indexes_.find(columnName) != indexes_.end()) {
                    auto& index = indexes_[columnName];  // 获取非 const 引用
                    index[value].erase(i);  // 移除旧的索引
                    std::visit([&](const auto& newIndexValue) {
                        index[newIndexValue].insert(i);  // 插入新的索引值
                    }, newValue);
                }
            }
        }, field);
    }

    return updated;
}

bool MemTable::update(
    const std::string& columnName,         // 待更新的列
    const Field& newValue,                 // 新值
    const std::function<bool(const Field&)>& predicate)  // 动态的查询谓词
{
    std::unique_lock<std::shared_mutex> lock(mutex_);  // 使用写锁，确保线程安全
    bool updated = false;

    // 获取列索引
    size_t colIdx = getColumnIndex(columnName);

    // 1. 检查是否是主键列
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name == columnName && columns_[i].primaryKey) {
            // 主键查询，使用主键索引
            auto& index = primaryKeyIndex_;
            for (auto& [key, idx] : index) {
                bool match = std::visit([&](const auto& value) {
                    return predicate(value);
                }, key);

                if (match) {
                    // 更新主键对应的行
                    rows_[idx][colIdx] = newValue;
                    // 更新主键索引（移除旧的主键，插入新的主键）
                    index.erase(key);
                    std::visit([&](const auto& value) {
                        index[value] = idx;  // 更新主键索引
                    }, newValue);
                    updated = true;
                }
            }
            return updated;
        }
    }

    // 2. 检查其他索引
    auto indexIt = indexes_.find(columnName);
    if (indexIt != indexes_.end()) {
        auto& index = indexIt->second;
        for (auto& [key, idxSet] : index) {
            bool match = std::visit([&](const auto& value) {
                return predicate(value);
            }, key);

            if (match) {
                for (int idx : idxSet) {
                    // 更新符合条件的行
                    rows_[idx][colIdx] = newValue;
                    // 更新索引：移除旧的值并插入新的值
                    index.erase(key); // 移除旧的索引值
                    std::visit([&](const auto& value) {
                        index[value].insert(idx);  // 插入新的索引值
                    }, newValue);
                    updated = true;
                }
            }
        }
        return updated;
    }

    // 3. 全表扫描，遍历所有行
    for (size_t i = 0; i < rows_.size(); ++i) {
        const auto& field = rows_[i][colIdx];

        // 使用 std::visit 来根据 Field 类型检查条件
        std::visit([&](const auto& value) {
            if (predicate(value)) {
                // 更新行
                rows_[i][colIdx] = newValue;
                updated = true;

                // 如果更新了索引列，需要同步更新索引
                if (indexes_.find(columnName) != indexes_.end()) {
                    auto& index = indexes_[columnName];  // 获取非 const 引用
                    index[value].erase(i);  // 移除旧的索引
                    std::visit([&](const auto& newIndexValue) {
                        index[newIndexValue].insert(i);  // 插入新的索引值
                    }, newValue);
                }
            }
        }, field);
    }

    return updated;
}

json MemTable::rowsToJson(const std::vector<Row>& rows) {
    json jsonRows = json::array();
    for (const auto& row : rows) {
        json rowJson;
        
        // Iterate through each column in the row based on the column definitions
        for (size_t i = 0; i < row.size(); ++i) {
            const auto& column = columns_[i];  // Get the column definition
            const auto& field = row[i];        // Get the value from the row
            
            // Add the field to the rowJson with the column's name as the key
            rowJson[column.name] = fieldToJson(field);
        }

        jsonRows.push_back(rowJson);
    }
    return jsonRows;
}

json MemTable::tableToJson() {
    std::shared_lock<std::shared_mutex> lock(mutex_); // 共享锁

    json jsonTable;
    jsonTable["name"] = name_;
    jsonTable["columns"] = columnsToJson(columns_); // 调用封装函数
    jsonTable["rows"] = rowsToJson(rows_);          // 调用封装函数
    return jsonTable;
}

json MemTable::showTable() {
    json jsonTable;
    jsonTable["name"] = name_;
    jsonTable["columns"] = columnsToJson(columns_);
    return jsonTable;
}

json MemTable::showRows() {
    std::shared_lock<std::shared_mutex> lock(mutex_); // 共享锁

    json jsonRows;
    jsonRows["rows"] = rowsToJson(rows_);
    return jsonRows;
}

void MemTable::exportToFile(const std::string& filePath) {
    // Write to file
    std::ofstream outFile(filePath);
    if (outFile.is_open()) {
        outFile << "{\"rows\":[\n"; // Start the JSON array

        std::string allRowsJson;
        
        for (size_t i = 0; i < rows_.size(); ++i) {
            const auto& row = rows_[i];
            json rowJson;

            // Iterate through each field in the row using column definitions
            for (size_t j = 0; j < row.size(); ++j) {
                const auto& column = columns_[j]; // Get the column definition
                const auto& field = row[j];       // Get the field value from the row
                
                // Add the field to the rowJson with the column's name as the key
                rowJson[column.name] = fieldToJson(field);
            }

            // Accumulate the row's JSON in the allRowsJson string
            allRowsJson += rowJson.dump();
            if (i < rows_.size() - 1) { // Avoid trailing comma
                allRowsJson += ",\n";
            }
        }

        // Write the accumulated JSON to file all at once
        outFile << allRowsJson;
        
        outFile << "\n]}"; // End the JSON array
        outFile.close();
    } else {
        throw std::runtime_error("Failed to open file for writing: " + filePath);
    }    
}


void MemTable::importRowsFromFile(const std::string& filePath) {
    std::ifstream inputFile(filePath);

    if (!inputFile.is_open()) {
        throw std::runtime_error("Unable to open file: " + filePath);
    }

    // 创建一个 JSON 解析器，逐行解析
    json jsonData = json::parse(inputFile, nullptr, false);

    if (jsonData.is_discarded()) {
        throw std::runtime_error("Invalid JSON format in file: " + filePath);
    }

    // 预估行数以优化内存分配 (假设文件中的行数可以预估)
    if (jsonData.contains("rows") && jsonData["rows"].is_array()) {
        rows_.reserve(jsonData["rows"].size());
    } else {
        throw std::runtime_error("Invalid file format: missing 'rows' array.");
    }

    insertRowsFromJson(jsonData);
    // 显式尝试释放 jsonData 中未使用的内存
    jsonData.clear();
    inputFile.close();
}

void MemTable::importRowsFromFile(const std::string& filePath, size_t batchSize ) {
    std::ifstream inputFile(filePath, std::ios::in | std::ios::binary);

    if (!inputFile.is_open()) {
        throw std::runtime_error("Unable to open file: " + filePath);
    }

    // 读取整个文件到内存
    std::stringstream buffer;
    buffer << inputFile.rdbuf();
    std::string fileContent = buffer.str();
    
    inputFile.close(); // 关闭文件，文件内容已经全部加载到内存

    // 按行分割文件内容并逐行解析 JSON
    std::istringstream stream(fileContent);
    std::string line;
    std::getline(stream, line);  // 跳过 "{rows["
    // 逐行读取文件内容
    size_t rowsRead = 0;
    
    std::vector<Row> batchRows;
    batchRows.reserve(batchSize);
    
    // 读取每一行的 JSON 对象并分批处理
    while (std::getline(stream, line)) {
        if (line.empty()) continue;  // 跳过空行
        if (line == "]}") {  // 结束 "rows" 数组部分
            break;
        }
        // 去掉行尾的逗号
        if (line.back() == ',') {
            line.pop_back();  // 去掉末尾的逗号
        }
        try {
            json jsonRow = json::parse(line);  // 解析单行 JSON 数据
            batchRows.push_back(jsonToRow(jsonRow));
        } catch (const json::parse_error& e) {
            std::cerr << "Error parsing row: " << e.what() << std::endl;
        }

        // 如果当前批次已达到指定的大小，插入数据并清空批次
        if (batchRows.size() >= batchSize) {
            this->insertRows(batchRows);
            batchRows.clear();  // 清空当前批次
            batchRows.shrink_to_fit();
        }

        rowsRead++;
        if (rowsRead % 1000 == 0) {
            std::cout << ".";
            std::cout.flush();
        }
    }

    // 插入最后一批未满批次的行
    if (!batchRows.empty()) {
        this->insertRows(batchRows);
        batchRows.clear();
        batchRows.shrink_to_fit();
    }
    std::cout << std::endl;
    inputFile.close();
}

