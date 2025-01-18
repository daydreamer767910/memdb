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
                primaryKeyIndex_[value] = rows_.size() - 1;  // 或者插入行号、唯一标识符等
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

int MemTable::insertRowsFromJson(const nlohmann::json& jsonRows) {
    std::lock_guard<std::mutex> lock(mutex_);
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
    std::lock_guard<std::mutex> lock(mutex_);
    Row newRow = processRowDefaults(row);
    if (validateRow(newRow) && validatePrimaryKey(newRow)) {
        rows_.push_back(newRow);
        updateIndexes(newRow, rows_.size() - 1);

        return true;
    }
    return false;
}


bool MemTable::insertRows(const std::vector<Row>& newRows) {
    std::lock_guard<std::mutex> lock(mutex_);
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
    return rows_;
}

size_t MemTable::getTotalRows() const{
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
    std::lock_guard<std::mutex> lock(mutex_);
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


// 获取从第 n 行开始的 limit 个数据
std::vector<Row> MemTable::getWithLimitAndOffset(int limit, int offset) const {
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

nlohmann::json MemTable::rowsToJson(const std::vector<Row>& rows) {
    nlohmann::json jsonRows = nlohmann::json::array();
    for (const auto& row : rows) {
        nlohmann::json rowJson;
        
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

nlohmann::json MemTable::tableToJson() {
    nlohmann::json jsonTable;
    jsonTable["name"] = name_;
    jsonTable["columns"] = columnsToJson(columns_); // 调用封装函数
    jsonTable["rows"] = rowsToJson(rows_);          // 调用封装函数
    return jsonTable;
}

nlohmann::json MemTable::showTable() {
    nlohmann::json jsonTable;
    jsonTable["name"] = name_;
    jsonTable["columns"] = columnsToJson(columns_);
    return jsonTable;
}

nlohmann::json MemTable::showRows() {
    nlohmann::json jsonRows;
    jsonRows["rows"] = rowsToJson(rows_);
    return jsonRows;
}

void MemTable::exportToFile(const std::string& filePath) {
    // Write to file
    std::ofstream outFile(filePath);
    if (outFile.is_open()) {
        outFile << "{\n\"rows\":\n["; // Start the JSON array

        for (size_t i = 0; i < rows_.size(); ++i) {
            const auto& row = rows_[i];
            nlohmann::json rowJson;

            // Iterate through each field in the row using column definitions
            for (size_t j = 0; j < row.size(); ++j) {
                const auto& column = columns_[j]; // Get the column definition
                const auto& field = row[j];       // Get the field value from the row
                
                // Add the field to the rowJson with the column's name as the key
                rowJson[column.name] = fieldToJson(field);
            }

            outFile << rowJson.dump(); // Write each row without formatting
            if (i < rows_.size() - 1) { // Avoid trailing comma
                outFile << ",";
            }
        }

        outFile << "]\n}"; // End the JSON array
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

    // 将文件内容读取到std::string中
    std::stringstream buffer;
    buffer << inputFile.rdbuf();

    // Parse the JSON content
    nlohmann::json jsonData = nlohmann::json::parse(buffer.str());

    if (jsonData.contains("rows") && jsonData["rows"].is_array()) {
        // Iterate over the rows in the JSON file
        for (const auto& rowJson : jsonData["rows"]) {
            Row row;

            // Iterate through each column in the row
            for (size_t i = 0; i < columns_.size(); ++i) {
                const auto& column = columns_[i];
                if (rowJson.contains(column.name)) {
                    // Deserialize the field value
                    row.push_back(jsonToField(column.type, rowJson[column.name]));
                } else {
                    // If the column is missing, you can handle it as needed (e.g., add a default value)
                    if (column.type == "date") {
                        row.push_back(getDefault(column.type));
                    } else {
                        row.push_back(column.defaultValue);
                    }
                }
            }
            validatePrimaryKey(row);
            // Add the row to rows_
            rows_.push_back(row);
        }
    } else {
        throw std::runtime_error("Invalid file format: missing 'rows' array.");
    }

    inputFile.close();
}


