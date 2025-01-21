#include "collection.hpp"
#include "util/util.hpp"

Table::Table(const std::string& tableName, const std::vector<Column>& columns)
    : name_(tableName), columns_(columns) {
    if (name_.empty() || columns_.empty()) {
        throw std::invalid_argument("Invalid table structure.");
    }
    for (const auto& column : columns_) {
        if (column.name.empty() || column.type.empty()) {
            throw std::invalid_argument("Each column must have a name and a type.");
        }
    }
}

bool Table::validateRow(const Row& row) {
    for (const auto& column : columns_) {
        auto it = row.find(column.name);
        if (it == row.end() && !column.nullable) {
            throw std::invalid_argument("Missing required field: " + column.name);
        }
        if (it != row.end() && !isValidType(it->second, column.type)) {
            throw std::invalid_argument("Invalid type for field: " + column.name);
        }
    }
    return true;
}

bool Table::validatePrimaryKey(const Row& row) {
    for (const auto& column : columns_) {
        if (column.primaryKey) {
            auto primaryKeyValue = row.at(column.name);

            // 检查主键是否存在于索引中
            if (primaryKeyIndex_.find(primaryKeyValue) != primaryKeyIndex_.end()) {
                throw std::invalid_argument("Primary key value already exists: " + column.name);
            }
        }
    }
    return true;
}

Row Table::processRowDefaults(const Row& row) const{
    Row newRow = row;
    for (const auto& column : columns_) {
        if (newRow.find(column.name) == newRow.end()) {
            if (column.type == "date") {
                newRow[column.name] = getDefault(column.type);
            } else {
                newRow[column.name] = column.defaultValue;
            }
        }
    }
    return newRow;
}

int Table::insertRowsFromJson(const json& jsonRows) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<size_t> newIndexes; // 记录需要更新索引的行
    int i = 0;
    for (const auto& jsonRow : jsonRows["rows"]) {
        Row row;
        for (const auto& [key, value] : jsonRow.items()) {
            auto columnIt = std::find_if(columns_.begin(), columns_.end(), [&key](const Column& col) {
                return col.name == key;
            });

            if (columnIt != columns_.end()) {
                row[key] = jsonToField(columnIt->type,value);
            }
        }
        Row newRow = processRowDefaults(row);
        if (validateRow(newRow) && validatePrimaryKey(newRow)) {
            rows_.push_back(newRow);
            newIndexes.push_back(rows_.size() - 1);
            i++;
            // 如果有主键，更新主键索引
            for (const auto& column : columns_) {
                if (column.primaryKey) {
                    auto primaryKeyValue = newRow.at(column.name);
                    primaryKeyIndex_[primaryKeyValue] = rows_.size() - 1;
                }
            }
        } 
    }

    // 批量更新索引
    updateIndexesBatch(newIndexes);
    //std::cout << "insert " << i << " rows into table[" << this->name << "] successfully!" << std::endl;
    return i;
}

bool Table::insertRow(const Row& row) {
    std::lock_guard<std::mutex> lock(mutex_);
    Row newRow = processRowDefaults(row);
    if (validateRow(newRow) && validatePrimaryKey(newRow)) {
        rows_.push_back(newRow);
        updateIndexes(newRow, rows_.size() - 1);

        // 如果有主键，更新主键索引
        for (const auto& column : columns_) {
            if (column.primaryKey) {
                auto primaryKeyValue = newRow.at(column.name);
                primaryKeyIndex_[primaryKeyValue] = rows_.size() - 1;
            }
        }
        return true;
    }
    return false;
}

bool Table::insertRows(const std::vector<Row>& newRows) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<size_t> newIndexes; // 记录需要更新索引的行
    for (const auto& row : newRows) {
        Row newRow = processRowDefaults(row);
        if (validateRow(newRow) && validatePrimaryKey(newRow)) {
            rows_.push_back(newRow);
            newIndexes.push_back(rows_.size() - 1);

            // 如果有主键，更新主键索引
            for (const auto& column : columns_) {
                if (column.primaryKey) {
                    auto primaryKeyValue = newRow.at(column.name);
                    primaryKeyIndex_[primaryKeyValue] = rows_.size() - 1;
                }
            }

        } else {
            return false;
        }
    }
    // 批量更新索引
    updateIndexesBatch(newIndexes);
    return true;
}

std::vector<Row> Table::getRows() const{
    return rows_;
}

size_t Table::getTotalRows() const{
    return rows_.size();
}

void Table::showIndexs() {
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

void Table::updateIndexes(const Row& row, int rowIndex) {
    for (const auto& [colName, index] : indexes_) {
        if (row.find(colName) != row.end()) {
            indexes_[colName][row.at(colName)].insert(rowIndex);

            // 打印索引更新信息
            std::cout << "Updated index for column: " << colName << "\n";
            std::cout << "  Value: ";
            std::visit([](auto&& val) { std::cout << val; }, row.at(colName));
            std::cout << ", Row Index: " << rowIndex << "\n";
        }
    }
}

void Table::updateIndexesBatch(const std::vector<size_t>& rowIdxes) {
    for (auto index : rowIdxes) {
        updateIndexes(rows_[index], index);
    }
}

void Table::addIndex(const std::string& columnName) {
    std::lock_guard<std::mutex> lock(mutex_);
    Index index;
    for (int i = 0; i < static_cast<int>(rows_.size()); ++i) {
        if (rows_[i].find(columnName) != rows_[i].end()) {
            index[rows_[i].at(columnName)].insert(i);
        }
    }
    indexes_[columnName] = index;
}

std::vector<Row> Table::queryByIndex(const std::string& columnName, const Field& value) const {
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
std::vector<Row> Table::getWithLimitAndOffset(int limit, int offset) const {
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

// 封装行的序列化逻辑
json Table::rowsToJson(const std::vector<Row>& rows) {
    json jsonRows = json::array();
    for (const auto& row : rows) {
        json rowJson;
        for (const auto& [key, value] : row) {
            rowJson[key] = fieldToJson(value);
        }
        jsonRows.push_back(rowJson);
    }
    return jsonRows;
}

json Table::tableToJson() {
    json jsonTable;
    jsonTable["name"] = name_;
    jsonTable["columns"] = columnsToJson(columns_); // 调用封装函数
    jsonTable["rows"] = rowsToJson(rows_);          // 调用封装函数
    return jsonTable;
}

json Table::showTable() {
    json jsonTable;
    jsonTable["name"] = name_;
    jsonTable["columns"] = columnsToJson(columns_);
    return jsonTable;
}

json Table::showRows() {
    json jsonRows;
    jsonRows["rows"] = rowsToJson(rows_);
    return jsonRows;
}

void Table::exportToFile(const std::string& filePath) {
    // Write to file
    std::ofstream outFile(filePath);
    if (outFile.is_open()) {
        outFile << "{\n\"rows\":\n["; // Start the JSON array

        for (size_t i = 0; i < rows_.size(); ++i) {
            const auto& row = rows_[i];
            json rowJson;
            for (const auto& [key, value] : row) {
                rowJson[key] = fieldToJson(value);
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

void Table::importRowsFromFile(const std::string& filePath) {
    std::ifstream inputFile(filePath);

    if (!inputFile.is_open()) {
        throw std::runtime_error("Unable to open file: " + filePath);
    }
    // 将文件内容读取到std::string中
    std::stringstream buffer;
    buffer << inputFile.rdbuf();
    //std::cout << "read file ok " << get_timestamp() << std::endl;
    json jsonData = json::parse(buffer.str());
    //std::cout << "parse buffer ok " << get_timestamp() << std::endl;
    insertRowsFromJson(jsonData);
    //std::cout << "insert table ok " << get_timestamp() << std::endl;
    /*[[maybe_unused]] auto result = json::parse(inputFile, [](int depth, json::parse_event_t event, json& parsed) {
        switch (event) {
            case json::parse_event_t::key:
                std::cout << "Key: " << parsed << " (Depth: " << depth << ")\n";
                break;
            case json::parse_event_t::value:
                std::cout << "Value: " << parsed << " (Depth: " << depth << ")\n";
                break;
            case json::parse_event_t::array_start:
                std::cout << "Start of array (Depth: " << depth << ")\n";
                break;
            case json::parse_event_t::array_end:
                std::cout << "End of array (Depth: " << depth << ")\n";
                break;
            case json::parse_event_t::object_start:
                std::cout << "Start of object (Depth: " << depth << ")\n";
                break;
            case json::parse_event_t::object_end:
                std::cout << "End of object (Depth: " << depth << ")\n";
                break;
            default:
                break;
        }
        // Continue parsing
        return true;
    });
    json::parser_callback_t callback = [this](int depth, json::parse_event_t event, json& parsed) {
        if (depth == 2 && event == json::parse_event_t::object_end) {
            Row row;
            for (const auto& [key, value] : parsed.items()) {
                std::cout << "Key: " << key << " Value: " << value << "\n";
                auto columnIt = std::find_if(columns.begin(), columns.end(), [&key](const Column& col) {
                    return col.name == key;
                });

                if (columnIt != columns.end()) {
                    row[key] = jsonToField(columnIt->type,value);
                }
            }
            insertRow(row);
        }
        return true; // Continue parsing
    };

    [[maybe_unused]] auto result = json::parse(inputFile, callback);*/
    inputFile.close();
}

