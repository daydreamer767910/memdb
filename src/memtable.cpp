#include "memtable.hpp"
#include "util.hpp"

MemTable::MemTable(const std::string& tableName, const std::vector<Column>& columns)
    : name(tableName), columns(columns) {
    if (name.empty() || columns.empty()) {
        throw std::invalid_argument("Invalid table structure.");
    }
    for (const auto& column : columns) {
        if (column.name.empty() || column.type.empty()) {
            throw std::invalid_argument("Each column must have a name and a type.");
        }
    }
}

bool MemTable::validateRow(const Row& row) {
    for (const auto& column : columns) {
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

bool MemTable::validatePrimaryKey(const Row& row) {
    for (const auto& column : columns) {
        if (column.primaryKey) {
            auto primaryKeyValue = row.at(column.name);
            for (const auto& existingRow : rows) {
                if (existingRow.at(column.name) == primaryKeyValue) {
                    throw std::invalid_argument("Primary key value already exists.");
                }
            }
        }
    }
    return true;
}

Row MemTable::processRowDefaults(const Row& row) {
    Row newRow = row;
    for (const auto& column : columns) {
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

void MemTable::insertRowsFromJson(const std::string& jsonString) {
    auto root = nlohmann::json::parse(jsonString);

    for (const auto& jsonRow : root["rows"]) {
        Row row;
        for (const auto& [key, value] : jsonRow.items()) {
            auto columnIt = std::find_if(columns.begin(), columns.end(), [&key](const Column& col) {
                return col.name == key;
            });

            if (columnIt != columns.end()) {
                row[key] = jsonToField(columnIt->type,value);
            }
        }
        insertRow(row);
    }
}

bool MemTable::insertRow(const Row& row) {
    Row newRow = processRowDefaults(row);
    if (validateRow(newRow) && validatePrimaryKey(newRow)) {
        rows.push_back(newRow);
        updateIndexes(newRow, rows.size() - 1);
        return true;
    }
    return false;
}

bool MemTable::insertRows(const std::vector<Row>& newRows) {
    std::vector<size_t> newIndexes; // 记录需要更新索引的行
    for (const auto& row : newRows) {
        Row newRow = processRowDefaults(row);
        if (validateRow(newRow) && validatePrimaryKey(newRow)) {
            rows.push_back(newRow);
            newIndexes.push_back(rows.size() - 1);
        } else {
            return false;
        }
    }
    // 批量更新索引
    updateIndexesBatch(newIndexes);
    return true;
}

std::vector<Row> MemTable::getRows() {
    return rows;
}

void MemTable::showIndexs() {
    for (const auto& [colName, index] : indexes) {
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
    for (const auto& [colName, index] : indexes) {
        if (row.find(colName) != row.end()) {
            indexes[colName][row.at(colName)].insert(rowIndex);

            // 打印索引更新信息
            std::cout << "Updated index for column: " << colName << "\n";
            std::cout << "  Value: ";
            std::visit([](auto&& val) { std::cout << val; }, row.at(colName));
            std::cout << ", Row Index: " << rowIndex << "\n";
        }
    }
}

void MemTable::updateIndexesBatch(const std::vector<size_t>& rowIdxes) {
    for (auto index : rowIdxes) {
        updateIndexes(rows[index], index);
    }
}

void MemTable::addIndex(const std::string& columnName) {
    Index index;
    for (int i = 0; i < rows.size(); ++i) {
        if (rows[i].find(columnName) != rows[i].end()) {
            index[rows[i].at(columnName)].insert(i);
        }
    }
    indexes[columnName] = index;
}

std::vector<Row> MemTable::queryByIndex(const std::string& columnName, const Field& value) {
    std::vector<Row> result;
    if (indexes.find(columnName) != indexes.end()) {
        //std::cout << "index for column: " << columnName << " with value:" << fieldToJson(value).dump() << std::endl;
        const auto& index = indexes[columnName];
        auto it = index.find(value);
        if (it != index.end()) {
            for (const auto& rowIndex : it->second) {
                //std::cout << rowIndex << std::endl;
                result.push_back(rows[rowIndex]);
            }
        }
    }
    //std::cout << rowsToJson(result).dump(4) << std::endl;
    return result;
}

// 封装列的序列化逻辑
nlohmann::json MemTable::columnsToJson(const std::vector<Column>& columns) {
    nlohmann::json jsonColumns = nlohmann::json::array();
    for (const auto& column : columns) {
        nlohmann::json colJson;
        colJson["name"] = column.name;
        colJson["type"] = column.type;
        colJson["nullable"] = column.nullable;
        colJson["defaultValue"] = fieldToJson(column.defaultValue);
        colJson["primaryKey"] = column.primaryKey;
        jsonColumns.push_back(colJson);
    }
    return jsonColumns;
}

// 封装行的序列化逻辑
nlohmann::json MemTable::rowsToJson(const std::vector<Row>& rows) {
    nlohmann::json jsonRows = nlohmann::json::array();
    for (const auto& row : rows) {
        nlohmann::json rowJson;
        for (const auto& [key, value] : row) {
            rowJson[key] = fieldToJson(value);
        }
        jsonRows.push_back(rowJson);
    }
    return jsonRows;
}

nlohmann::json MemTable::tableToJson() {
    nlohmann::json jsonTable;
    jsonTable["name"] = name;
    jsonTable["columns"] = columnsToJson(columns); // 调用封装函数
    jsonTable["rows"] = rowsToJson(rows);          // 调用封装函数
    return jsonTable;
}

void MemTable::showTable() {
    nlohmann::json jsonTable;
    jsonTable["name"] = name;
    jsonTable["columns"] = columnsToJson(columns);
    std::cout << jsonTable.dump(4) << std::endl;
}

void MemTable::showRows() {
    nlohmann::json jsonRows;
    jsonRows["rows"] = rowsToJson(rows);
    std::cout << jsonRows.dump(4) << std::endl;
}

void MemTable::exportToFile(const std::string& filePath) {
    nlohmann::json jsonTable = tableToJson();

    std::vector<uint8_t> packet = pack_data(jsonTable,1);
    // Write to file
    std::ofstream outFile(filePath);
    if (outFile.is_open()) {
        outFile << packet;
        //outFile << jsonTable.dump(4); // Pretty print with 4-space indentation
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
    /*[[maybe_unused]] auto result = nlohmann::json::parse(inputFile, [](int depth, nlohmann::json::parse_event_t event, nlohmann::json& parsed) {
        switch (event) {
            case nlohmann::json::parse_event_t::key:
                std::cout << "Key: " << parsed << " (Depth: " << depth << ")\n";
                break;
            case nlohmann::json::parse_event_t::value:
                std::cout << "Value: " << parsed << " (Depth: " << depth << ")\n";
                break;
            case nlohmann::json::parse_event_t::array_start:
                std::cout << "Start of array (Depth: " << depth << ")\n";
                break;
            case nlohmann::json::parse_event_t::array_end:
                std::cout << "End of array (Depth: " << depth << ")\n";
                break;
            case nlohmann::json::parse_event_t::object_start:
                std::cout << "Start of object (Depth: " << depth << ")\n";
                break;
            case nlohmann::json::parse_event_t::object_end:
                std::cout << "End of object (Depth: " << depth << ")\n";
                break;
            default:
                break;
        }
        // Continue parsing
        return true;
    });*/
    nlohmann::json::parser_callback_t callback = [this](int depth, nlohmann::json::parse_event_t event, nlohmann::json& parsed) {
        if (depth == 2 && event == nlohmann::json::parse_event_t::object_end) {
            Row row;
            for (const auto& [key, value] : parsed.items()) {
                //std::cout << "Key: " << key << " Value: " << value << "\n";
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

    [[maybe_unused]] auto result = nlohmann::json::parse(inputFile, callback);
    inputFile.close();
}

