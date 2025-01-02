#include "memdatabase.hpp"

// Add a new table to the database
void MemDatabase::addTable(const std::string& tableName, const std::vector<Column>& columns) {
    if (tables.find(tableName) != tables.end()) {
        throw std::invalid_argument("Table " + tableName + " already exists.");
    }
    tables.emplace(tableName, std::make_shared<MemTable>(tableName, columns));
}

void MemDatabase::addTableFromJson(const std::string& jsonConfig) {
    auto root = nlohmann::json::parse(jsonConfig);

    std::string tableName = root["name"];
    auto columns = jsonToColumns(root);
    addTable(tableName, columns);
}

void MemDatabase::addTableFromFile(const std::string& filePath) {
    std::ifstream inputFile(filePath);
    if (!inputFile.is_open()) {
        throw std::runtime_error("Unable to open file: " + filePath);
    }
    // 将文件内容读取到std::string中
    std::stringstream buffer;
    buffer << inputFile.rdbuf();
    std::string fileContent = buffer.str();
    addTableFromJson(fileContent);
    inputFile.close();
}

// Retrieve a table by its name
MemTable::ptr MemDatabase::getTable(const std::string& tableName) {
    auto it = tables.find(tableName);
    return it != tables.end() ? it->second : nullptr;
}

// Update an existing table
void MemDatabase::updateTable(const MemTable& updatedTable) {
    auto it = tables.find(updatedTable.name);
    if (it == tables.end()) {
        throw std::invalid_argument("Table " + updatedTable.name + " does not exist.");
    }
    it->second = std::make_shared<MemTable>(updatedTable);
}

// Remove a table by its name
void MemDatabase::removeTable(const std::string& tableName) {
    auto it = tables.find(tableName);
    if (it == tables.end()) {
        throw std::invalid_argument("Table " + tableName + " does not exist.");
    }
    tables.erase(it);
}

// List all table names in the database
std::vector<std::string> MemDatabase::listTableNames() const {
    std::vector<std::string> tableNames;
    for (const auto& table : tables) {
        tableNames.push_back(table.first);
    }
    return tableNames;
}

// Get all table instances in the database
std::vector<MemTable::ptr> MemDatabase::listTables() const {
    std::vector<MemTable::ptr> tableInstances;
    for (const auto& table : tables) {
        tableInstances.push_back(table.second);
    }
    return tableInstances;
}

