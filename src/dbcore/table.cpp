#include "table.hpp"
#include "util/util.hpp"

void Table::fromJson(const json& j) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    name_ = j.at("name").get<std::string>();
    columns_ = jsonToColumns(j);
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

bool Table::isValidType(const FieldValue& value, const std::string& type) const{
    if (type == "array") {
        return std::holds_alternative<std::vector<uint8_t>>(value);
    }
    if (type == "date") {
        return std::holds_alternative<std::time_t>(value);
    }
    if (type == "int") {
        return std::holds_alternative<int>(value);
    }
    if (type == "bool") {
        return std::holds_alternative<bool>(value);
    }
    if (type == "double") {
        return std::holds_alternative<double>(value);
    }
    if (type == "string") {
        return std::holds_alternative<std::string>(value);
    }

    return false; // 默认返回 false
}

bool Table::validateRow(const Row& row) {
    if (row.size() != columns_.size()) {
        throw std::invalid_argument("Row size does not match column count.");
    }
    for (size_t i = 0; i < columns_.size(); ++i) {
        const auto& column = columns_[i];
        const auto& value = row[i].getValue();
        if (value.index() == 0 && !column.nullable) {
            throw std::invalid_argument("Missing required FieldValue: " + column.name);
        }
        if (!isValidType(value, column.type)) {
            throw std::invalid_argument("Invalid type for FieldValue: " + column.name);
        }
    }
    return true;
}


bool Table::validatePrimaryKey(const Row& row) {
    size_t currentRowIdx = rows_.size();  // 获取当前行号
    for (size_t i = 0; i < columns_.size(); ++i) {
        const auto& column = columns_[i];
        if (column.primaryKey) {
            // 获取主键值，基于列索引
            const auto& field = row[i];

            // 检查主键值是否存在于索引中
            //std::visit([&](const auto& value) {
                // 如果主键值已经存在于索引中，抛出异常
                if (primaryKeyIndex_.find(field) != primaryKeyIndex_.end()) {
                    throw std::invalid_argument("Primary key value already exists: " + column.name);
                }
                // 如果主键值没有重复，插入主键值到主键索引中
                primaryKeyIndex_[field] = currentRowIdx;  // 使用当前行号
            //}, field);
        }
    }
    return true;
}

Row Table::processRowDefaults(const Row& row) const {
    Row newRow;
    size_t rowIndex = 0;  // 用来追踪当前 row 中的列位置

    // 遍历每个列，检查列是否在 row 中有对应的字段
    for (size_t i = 0; i < columns_.size(); ++i) {
        const auto& column = columns_[i];
        
        // 检查 row 中是否有对应的字段，如果 row 中的列数不足，就插入默认值
        if (rowIndex < row.size()) {
            const auto& field = row[rowIndex];
            if (isValidType(field.getValue(), column.type)) {
                // 如果 row 中有值并且类型匹配，直接插入
                newRow.push_back(field);
            } else {
                // 如果类型不匹配，用默认值
                //newRow.push_back(getDefault(column.type));
                if (column.type == "date") {
                    newRow.push_back(Field(getDefault(column.type)));
                } else {
                    newRow.push_back(Field(column.defaultValue));
                }
            }
            rowIndex++;
        } else {
            // 如果 row 中没有更多的列，插入该列的默认值
            //newRow.push_back(getDefault(column.type));
            if (column.type == "date") {
                newRow.push_back(Field(getDefault(column.type)));
            } else {
                newRow.push_back(Field(column.defaultValue));
            }
        }
    }
    
    return newRow;
}

Row Table::jsonToRow(const json& jsonRow) {
    Row row(columns_.size());  // 初始化一个 Row，大小为 columns_ 的大小

    // 遍历 JSON 对象的所有字段
    for (const auto& [key, value] : jsonRow.items()) {
        // 找到对应的列
        auto columnIt = std::find_if(columns_.begin(), columns_.end(), [&key](const Column& col) {
            return col.name == key;
        });
        
        if (columnIt != columns_.end()) {
            Field field;
            field.fromJson(columnIt->type, value);
            size_t index = std::distance(columns_.begin(), columnIt);  // 获取列的索引
            row[index] = field;  // 使用列类型转换字段
        }
    }

    return row;
}

std::vector<Row> Table::jsonToRows(const json& jsonRows) {
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
                Field field;
                field.fromJson(columns_[index].type, value);// 使用索引填充行
                row[index] = field;  
            }
        }

        rows.push_back(row);
    }

    return rows;
}



int Table::insertRowsFromJson(const json& jsonRows) {
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
                Field field;
                field.fromJson(columnIt->type, value);
                size_t index = std::distance(columns_.begin(), columnIt);  // 获取列的索引
                row[index] = field;
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

bool Table::insertRow(const Row& row) {
    std::unique_lock<std::shared_mutex> lock(mutex_); // 独占锁
    Row newRow = processRowDefaults(row);
    if (validateRow(newRow) && validatePrimaryKey(newRow)) {
        rows_.push_back(newRow);
        updateIndexes(newRow, rows_.size() - 1);

        return true;
    }
    return false;
}


bool Table::insertRows(const std::vector<Row>& newRows) {
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

std::vector<Row> Table::getRows() const{
    std::shared_lock<std::shared_mutex> lock(mutex_); // 共享锁
    return rows_;
}

size_t Table::getTotalRows() const{
    std::shared_lock<std::shared_mutex> lock(mutex_); // 共享锁
    return rows_.size();
}

void Table::updateIndexes(const Row& row, int rowIndex) {
    for (size_t i = 0; i < columns_.size(); ++i) {
        const auto& column = columns_[i];
        if (column.indexed) {
            auto& index = indexes_[column.name];
            // 获取当前列的值
            auto columnValue = row[i];

            // 更新索引
            index[columnValue].insert(rowIndex);

            // 打印索引更新信息
            //std::cout << "Updated index for column: " << column.name << "\n";
            //std::cout << "  Value: ";
            //std::visit([](auto&& val) { std::cout << val; }, columnValue.getValue();
            //std::cout << "  json Value: " << columnValue;
            //std::cout << ", Row Index: " << rowIndex << "\n";
        }
    }
}

void Table::updateIndexesBatch(const std::vector<size_t>& rowIdxes) {
    for (auto index : rowIdxes) {
        updateIndexes(rows_[index], index);
    }
}

void Table::buildIndex() {
    // 遍历所有列，检查哪些列需要索引
    for (size_t colIdx = 0; colIdx < columns_.size(); ++colIdx) {
        const auto& column = columns_[colIdx];
        
        if (column.indexed) {
            // 该列需要索引，重新构建索引
            for (size_t rowIdx = 0; rowIdx < rows_.size(); ++rowIdx) {
                const auto& field = rows_[rowIdx][colIdx];
                
                // 索引映射：字段值 -> 行号
                indexes_[column.name][field].insert(rowIdx);
            }
        }
    }
}


// 获取从第 n 行开始的 limit 个数据
std::vector<Row> Table::getWithLimitAndOffset(int limit, int offset) const {
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

size_t Table::getColumnIndex(const std::string& columnName) const {
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name == columnName) {
            return i;
        }
    }
    throw std::invalid_argument("Column not found: " + columnName);
}

std::string Table::getColumnType(const std::string& columnName) const {
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name == columnName) {
            return columns_[i].type;
        }
    }
    throw std::invalid_argument("Column not found: " + columnName);
}

std::vector<std::string> Table::getColumnTypes(const std::vector<std::string>& columnNames) const {
    std::vector<std::string> types;
    types.reserve(columnNames.size());

    for (const auto& columnName : columnNames) {
        bool found = false;

        for (const auto& column : columns_) {
            if (column.name == columnName) {
                types.push_back(column.type);
                found = true;
                break;
            }
        }

        if (!found) {
            throw std::invalid_argument("Column not found: " + columnName);
        }
    }

    return types;
}

bool Table::isPrimaryKey(const std::string& columnName) const {
    // 遍历 columns_ 查找该列名是否为主键
    for (const auto& column : columns_) {
        if (column.name == columnName) {
            return column.primaryKey;  // 返回是否为主键
        }
    }
    throw std::invalid_argument("Column not found: " + columnName);  // 如果列名不存在，抛出异常
}

std::vector<size_t> Table::matchPrimaryKey(
    const std::vector<size_t>& rowSet,
    const FieldValue& queryValue,
    const std::string& op,
    const std::string& columnName
) const {
    std::vector<size_t> matchedRows;

    if (!rowSet.empty()) {
        //只遍历rowSet
        for (size_t rowIndex : rowSet) {
            auto it = primaryKeyIndex_.find(rows_[rowIndex][getColumnIndex(columnName)]);
            if (it != primaryKeyIndex_.end()) {
                // 检查主键值是否符合条件
                if (std::visit([&](const auto& value) {
                    return compare(value, queryValue, op);
                }, it->first.getValue())) {
                    matchedRows.push_back(rowIndex);
                }
            }
        }
    } else {
        // 遍历主键索引，查找符合条件的主键
        for (const auto& [key, rowIndex] : primaryKeyIndex_) {
            // 使用 compare 来判断主键值是否符合条件
            if (std::visit([&](const auto& value) {
                return compare(value, queryValue, op);
            }, key.getValue())) {
                matchedRows.push_back(rowIndex);  // 将符合条件的行索引添加到结果
            }
        }
    }
    
    return matchedRows;
}

std::vector<size_t> Table::matchIndex(
    const std::vector<size_t>& rowSet,
    const FieldValue& queryValue,
    const std::string& op,
    const std::string& columnName
) const {
    std::vector<size_t> matchedRows;

    // 获取对应列的索引
    const auto& index = indexes_.at(columnName);

    // 遍历 rowSet 中的每一行
    for (size_t rowIndex : rowSet) {
        // 获取该行的索引列值
        const auto& fieldValue = rows_[rowIndex][getColumnIndex(columnName)].getValue();

        // 使用模板比较函数，判断查询值与索引值是否匹配
        bool conditionMatch = std::visit([&](const auto& value) {
            return compare(value, queryValue, op);
        }, fieldValue);

        // 如果匹配，将行索引加入结果
        if (conditionMatch) {
            matchedRows.push_back(rowIndex);
        }
    }

    return matchedRows;
}

std::vector<size_t> Table::search(
    const std::vector<std::string>& conditions,   // 查询条件列
    const std::vector<FieldValue>& queryValues,        // 查询条件值
    const std::vector<std::string>& operators     // 比较操作符（对应每个条件）
) const
{
    std::vector<size_t> rowSet;  // 存储符合条件的行索引
    std::vector<size_t> result;
    // 验证输入参数的合法性
    if (conditions.size() != queryValues.size() || conditions.size() != operators.size()) {
        throw std::invalid_argument("conditions, queryValues and operators must have the same size.");
    }

    // 优先使用主键索引进行查询
    for (size_t i = 0; i < conditions.size(); ++i) {
        const std::string& columnName = conditions[i];
        if (isPrimaryKey(columnName)) {
            // 仅根据主键进行查询
            rowSet = matchPrimaryKey(rowSet, queryValues[i], operators[i], columnName);
            if (rowSet.empty())// 如果主键查询没有结果，直接返回空结果
                return result;
        }
    }

    // 如果没有主键查询结果，再进行索引查询
    if (rowSet.empty()) {
        // 填充所有行的索引
        for (size_t i = 0; i < rows_.size(); ++i) {
            rowSet.push_back(i);
        }
    }

    // 使用索引进行查询
    for (size_t i = 0; i < conditions.size(); ++i) {
        const std::string& columnName = conditions[i];
        if (columns_[getColumnIndex(columnName)].indexed) {
            // 使用索引查找符合条件的行
            rowSet = matchIndex(rowSet, queryValues[i], operators[i], columnName);
            if (rowSet.empty())// 如果索引查询没有结果，直接返回空结果
                return result;
        }
    }

    // 如果条件包含索引且没有结果，返回空的查询结果
    if (rowSet.empty()) {
        return result;
    }

    // 遍历 rowSet 中的所有行，检查每行是否满足所有查询条件
    for (size_t rowIdx : rowSet) {
        const auto& row = rows_[rowIdx];
        bool match = true;

        // 检查该行是否满足所有条件
        for (size_t condIdx = 0; condIdx < conditions.size(); ++condIdx) {
            if (isPrimaryKey(conditions[condIdx]) || columns_[getColumnIndex(conditions[condIdx])].indexed) 
            //主键和索引已经在上面检查过了
                continue;
            size_t colIdx = getColumnIndex(conditions[condIdx]);
            const auto& fieldValue = row[colIdx].getValue();
            const auto& queryValue = queryValues[condIdx];
            const auto& op = operators[condIdx];

            // 使用模板比较函数，判断查询值与索引值是否匹配
            bool conditionMatch = std::visit([&](const auto& value) {
                //std::cout << "v: " << value << " qv: " << queryValue << std::endl;
                return compare(value, queryValue, op);
            }, fieldValue);

            // 使用 std::visit 比较字段值
            if (!conditionMatch) {
                match = false;
                break;  // 如果某个条件不匹配，跳过该行
            }
        }

        // 如果该行满足所有条件，将其加入结果集
        if (match) {
            result.push_back(rowIdx);
        }
    }
    return result;
}

std::vector<std::vector<FieldValue>> Table::query(
    const std::vector<std::string>& columnNames,
    const std::vector<std::string>& conditions,   // 查询条件列
    const std::vector<FieldValue>& queryValues,        // 查询条件值
    const std::vector<std::string>& operators,     // 比较操作符（对应每个条件）
    int limit
) const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);  // 使用读锁，确保线程安全
    std::vector<std::vector<FieldValue>> result;           // 存储查询结果

    // 验证输入参数的合法性
    getColumnTypes(columnNames);
    getColumnTypes(conditions);

    std::cout << std::endl;
    std::vector<size_t> rowSet = search(conditions, queryValues, operators);

    // 遍历 rowSet 中的所有行
    for (size_t rowIdx : rowSet) {
        limit--;
        if (limit<0) break;
        std::vector<FieldValue> fieldValues;
        // 检查该行是否满足所有条件
        for (auto columnName :columnNames) {
            size_t colIdx = getColumnIndex(columnName);
            fieldValues.push_back(rows_[rowIdx][colIdx].getValue());
        }
        result.push_back(fieldValues);
    }

    return result;
}

size_t Table::update(
    const std::vector<std::string>& columnNames,
    const std::vector<FieldValue>& newValues,
    const std::vector<std::string>& conditions,
    const std::vector<FieldValue>& queryValues,
    const std::vector<std::string>& operators)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);  // 使用写锁，确保线程安全
    
    // 验证输入参数的合法性
    getColumnTypes(columnNames);
    getColumnTypes(conditions);
    // 验证输入参数的合法性
    if (columnNames.size() != newValues.size()) {
        throw std::invalid_argument("columnNames and newValues must have the same size.");
    }
    

    for (const auto& columnName : columnNames) {
        size_t colIdx = getColumnIndex(columnName);
        if (columns_[colIdx].primaryKey) {
            throw std::invalid_argument("Updating primary key is not allowed.");
        }
    }

    std::vector<size_t> rowSet = search(conditions, queryValues, operators);
    // 遍历 rowSet 中的所有行
    for (size_t rowIdx : rowSet) {
        for (size_t i = 0; i < columnNames.size(); ++i) {
            size_t colIdx = getColumnIndex(columnNames[i]);
            const auto& newValue = Field(newValues[i]);
            const auto& oldValue = rows_[rowIdx][colIdx];

            // 如果更新的是主键列
            if (columns_[colIdx].primaryKey) {
                // 检查主键唯一性（排除当前行）
                if (primaryKeyIndex_.find(newValue) != primaryKeyIndex_.end()) {
                    if (primaryKeyIndex_[newValue] != rowIdx) {
                        throw std::invalid_argument("Primary key uniqueness violated.");
                    }
                }
                auto& index = primaryKeyIndex_;
                index.erase(oldValue);  // 移除旧的主键值
                index[newValue] = rowIdx;  // 添加新的主键值
            }

            // 如果更新的是索引列
            if (indexes_.find(columnNames[i]) != indexes_.end()) {
                auto& index = indexes_[columnNames[i]];
                index[oldValue].erase(rowIdx);  // 从索引中移除旧值
                if (index[oldValue].empty()) {
                    index.erase(oldValue);  // 如果集合为空，移除该索引条目
                }
                index[newValue].insert(rowIdx);  // 添加新值到索引
            }

            // 更新实际数据
            rows_[rowIdx][colIdx] = newValue;
        }
    }

    return rowSet.size();
}

size_t Table::remove(
    const std::vector<std::string>& conditions,   // 查询条件列
    const std::vector<FieldValue>& queryValues,        // 查询条件值
    const std::vector<std::string>& operators     // 比较操作符（对应每个条件）
)
{
    std::unique_lock<std::shared_mutex> lock(mutex_); // 使用写锁，确保线程安全
    // 验证输入参数的合法性
    getColumnTypes(conditions);
    std::vector<size_t> rowSet = search(conditions, queryValues, operators);
    // 按降序排列 rowSet
    std::sort(rowSet.rbegin(), rowSet.rend());
    for (size_t rowIdx : rowSet) {
        // 更新主键索引
        for (size_t colIdx = 0; colIdx < columns_.size(); ++colIdx) {
            if (columns_[colIdx].primaryKey) {
                const auto& primaryKeyValue = rows_[rowIdx][colIdx];
                primaryKeyIndex_.erase(primaryKeyValue);
            }
        }

        // 更新列索引
        for (size_t colIdx = 0; colIdx < columns_.size(); ++colIdx) {
            const auto& columnName = columns_[colIdx].name;
            if (columns_[colIdx].indexed) {
                auto& index = indexes_[columnName];
                const auto& value = rows_[rowIdx][colIdx];
                index[value].erase(rowIdx);  // 从索引中移除
                if (index[value].empty()) {
                    index.erase(value);  // 如果集合为空，移除索引条目
                }
            }
        }
        // 从 rows_ 中删除行
        rows_.erase(rows_.begin()+rowIdx);
    }
    return rowSet.size();
}

json Table::rowsToJson(const std::vector<Row>& rows) {
    json jsonRows = json::array();
    for (const auto& row : rows) {
        json rowJson;
        
        // Iterate through each column in the row based on the column definitions
        for (size_t i = 0; i < row.size(); ++i) {
            const auto& column = columns_[i];  // Get the column definition
            const auto& field = row[i];        // Get the value from the row
            
            // Add the FieldValue to the rowJson with the column's name as the key
            rowJson[column.name] = field.toJson();
        }

        jsonRows.push_back(rowJson);
    }
    return jsonRows;
}

json Table::toJson() const{
    std::shared_lock<std::shared_mutex> lock(mutex_); // 共享锁

    json jsonTable;
    jsonTable["name"] = name_;
    jsonTable["columns"] = columnsToJson(columns_); // 调用封装函数
    //jsonTable["rows"] = rowsToJson(rows_);          // 调用封装函数
    return jsonTable;
}


json Table::showRows() {
    std::shared_lock<std::shared_mutex> lock(mutex_); // 共享锁

    json jsonRows;
    jsonRows["rows"] = rowsToJson(rows_);
    return jsonRows;
}

void Table::saveSchema(const std::string& filePath) {
    // 将表信息序列化为 JSON
    json root;
    root["name"] = name_;
    root["type"] = "table";
    root["columns"] = columnsToJson(columns_);

    // 将 JSON 写入文件
    std::ofstream outputFile(filePath);
    if (!outputFile.is_open()) {
        throw std::runtime_error("Unable to open file for writing: " + filePath);
    }
    outputFile << root.dump(4); // 格式化为缩进 4 的 JSON
    outputFile.close();
}

void Table::exportToBinaryFile(const std::string& filePath) {
    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filePath);
    }

    size_t rowsWrite = 0;
    size_t numRows = rows_.size();
    size_t numColumns = columns_.size();
    const size_t bufferSize = 32 * 1024; // 固定缓冲区大小为 128KB
    char* buffer = new char[bufferSize];
    size_t bufferUsed = 0;

    auto flushBuffer = [&]() {
        if (bufferUsed > 0) {
            outFile.write(buffer, bufferUsed);
            bufferUsed = 0;
        }
    };

    // 写入行数和列数
    if (bufferUsed + sizeof(numRows) + sizeof(numColumns) > bufferSize) {
        flushBuffer();
    }
    memcpy(buffer + bufferUsed, &numRows, sizeof(numRows));
    bufferUsed += sizeof(numRows);
    memcpy(buffer + bufferUsed, &numColumns, sizeof(numColumns));
    bufferUsed += sizeof(numColumns);

    // 写入数据
    for (const auto& row : rows_) {
        for (const auto& field : row) {
            // 假设 FieldValue 有一个 toBinary 方法，可以将自身序列化为二进制格式
            std::string binaryData = field.toBinary();
            size_t dataSize = binaryData.size();

            // 检查缓冲区是否有足够空间容纳新数据
            if (bufferUsed + sizeof(dataSize) + dataSize > bufferSize) {
                flushBuffer();
            }

            memcpy(buffer + bufferUsed, &dataSize, sizeof(dataSize));
            bufferUsed += sizeof(dataSize);
            memcpy(buffer + bufferUsed, binaryData.data(), dataSize);
            bufferUsed += dataSize;
        }
        rowsWrite++;
        if (rowsWrite % 10000 == 0) {
            std::cout << ".";
            std::cout.flush();
        }
    }

    // 刷新剩余缓冲区内容
    flushBuffer();

    std::cout << std::endl;
    delete[] buffer;
    outFile.close();
}


void Table::importFromBinaryFile(const std::string& filePath) {
    std::ifstream inFile(filePath, std::ios::binary);
    if (!inFile.is_open()) {
        throw std::runtime_error("Failed to open file for reading: " + filePath);
    }
    size_t numRows, numColumns;

    // 读取行数和列数
    inFile.read(reinterpret_cast<char*>(&numRows), sizeof(numRows));
    inFile.read(reinterpret_cast<char*>(&numColumns), sizeof(numColumns));

    // 检查文件格式是否匹配
    if (numColumns != columns_.size()) {
        throw std::runtime_error("Column count mismatch in binary file.");
    }
    rows_.clear();
    rows_.reserve(numRows);

    // 读取数据
    for (size_t i = 0; i < numRows; ++i) {
        Row row(numColumns);
        for (size_t j = 0; j < numColumns; ++j) {
            size_t dataSize;
            inFile.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));

            std::vector<char> buffer(dataSize);
            inFile.read(buffer.data(), dataSize);
            // 这里假设 FieldValue 有一个 fromBinary 方法，可以从二进制数据中解析自己
            Field field;
            field.fromBinary(buffer.data(), dataSize);
            row[j] = field;
        }
        //rows_.push_back(row);
        this->insertRow(row);
        if (i % 10000 == 0) {
            std::cout << ".";
            std::cout.flush();
        }
    }
    std::cout << std::endl;
    inFile.close();
    buildIndex();
}
/*
void Table::exportToFile(const std::string& filePath) {
    // Write to file
    std::ofstream outFile(filePath);
    size_t rowsWrite = 0;
    if (outFile.is_open()) {
        outFile << "{\"rows\":[\n"; // Start the JSON array

        std::string allRowsJson;
        
        for (size_t i = 0; i < rows_.size(); ++i) {
            const auto& row = rows_[i];
            json rowJson;

            // Iterate through each FieldValue in the row using column definitions
            for (size_t j = 0; j < row.size(); ++j) {
                const auto& column = columns_[j]; // Get the column definition
                const auto& field = row[j];       // Get the FieldValue value from the row
                
                // Add the FieldValue to the rowJson with the column's name as the key
                rowJson[column.name] = field.toJson();
            }

            // Accumulate the row's JSON in the allRowsJson string
            allRowsJson += rowJson.dump();
            if (i < rows_.size() - 1) { // Avoid trailing comma
                allRowsJson += ",\n";
            }

            rowsWrite++;
            if (rowsWrite % 1000 == 0) {
                std::cout << ".";
                std::cout.flush();
            }
        }
        std::cout << std::endl;
        // Write the accumulated JSON to file all at once
        outFile << allRowsJson;
        
        outFile << "\n]}"; // End the JSON array
        outFile.close();
    } else {
        throw std::runtime_error("Failed to open file for writing: " + filePath);
    }    
}


void Table::importFromFile(const std::string& filePath ) {
    std::ifstream inputFile(filePath, std::ios::in | std::ios::binary);

    if (!inputFile.is_open()) {
        throw std::runtime_error("Unable to open file: " + filePath);
    }
    size_t batchSize = 1024*10;
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
    buildIndex();
}
*/
