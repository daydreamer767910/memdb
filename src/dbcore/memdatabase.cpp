#include <iostream>
#include <filesystem>
#include "memdatabase.hpp"

// Add a new table to the database
void MemDatabase::addTable(const std::string& tableName, const std::vector<Column>& columns) {
    if (tables.find(tableName) != tables.end()) {
        throw std::invalid_argument("Table " + tableName + " already exists.");
    }
    tables.emplace(tableName, std::make_shared<MemTable>(tableName, columns));
}

void MemDatabase::addTableFromJson(const std::string& jsonConfig) {
    auto root = json::parse(jsonConfig);
    //std::cout << root.dump(4) << std::endl;
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
/*void MemDatabase::updateTable(MemTable&& updatedTable) {
    auto it = tables.find(updatedTable.name);
    if (it == tables.end()) {
        throw std::invalid_argument("Table " + updatedTable.name + " does not exist.");
    }
    it->second = std::make_shared<MemTable>(std::move(updatedTable));  // Use move semantics here
}*/

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

void MemDatabase::saveTableToFile(MemTable::ptr table, const std::string& filePath) {
    const std::string& tableName = table->name_;

    // 将表信息序列化为 JSON
    json root;
    root["name"] = tableName;
    root["columns"] = columnsToJson(table->columns_);

    // 将 JSON 写入文件
    std::ofstream outputFile(filePath);
    if (!outputFile.is_open()) {
        throw std::runtime_error("Unable to open file for writing: " + filePath);
    }
    outputFile << root.dump(4); // 格式化为缩进 4 的 JSON
    outputFile.close();
}


void MemDatabase::save(const std::string& filePath) {
    try {
        std::string baseDir = filePath;  // 基目录
        std::string subDir = "config";   // tables config
        std::filesystem::path fullPath = std::filesystem::path(baseDir) / subDir;
        // 创建目录及其所有父目录
        if (std::filesystem::create_directories(fullPath)) {
            //std::cout << "Successfully created directories: " << fullPath << '\n';
        } else {
            //std::cout << "Directories already exist: " << fullPath << '\n';
        }
        for (const auto table : tables) {
            std::filesystem::path tablePath = std::filesystem::path(fullPath) / table.first;
            saveTableToFile(table.second, tablePath.string());
        }


        subDir = "data"; //data
        fullPath = std::filesystem::path(baseDir) / subDir;
        // 创建目录及其所有父目录
        if (std::filesystem::create_directories(fullPath)) {
            //std::cout << "Successfully created directories: " << fullPath << '\n';
        } else {
            //std::cout << "Directories already exist: " << fullPath << '\n';
        }
        for (const auto table : tables) {
            std::filesystem::path tablePath = std::filesystem::path(fullPath) / table.first;
            std::cout << get_timestamp() << " start saving table[" << table.first << "]:\n";
            //table.second->exportToFile(tablePath.string());
            table.second->exportToBinaryFile(tablePath.string());
            std::cout << get_timestamp() << " done sucessfully! "  << std::endl;
        }

    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }
}

void MemDatabase::upload(const std::string& filePath) {
    try {
        std::string subDir = "config";   // 子目录
        std::filesystem::path fullPath = std::filesystem::path(filePath) / subDir;
        for (const auto& entry : std::filesystem::directory_iterator(fullPath)) {
            if (entry.is_regular_file()) { // 只打印文件（排除目录等其他类型）
                std::filesystem::path tblFile = std::filesystem::path(fullPath) / entry.path().filename();
                this->addTableFromFile(tblFile.string());
                //std::cout << "load table[" << entry.path().filename() << "] sucessfully!" << std::endl;
            }
        }
                
        subDir = "data";
        fullPath = std::filesystem::path(filePath) / subDir;
        for (const auto table : tables) {
            std::filesystem::path tablePath = std::filesystem::path(fullPath) / table.first;
            std::cout << get_timestamp() << " start loading table[" << table.first << "]:\n";
            //table.second->importRowsFromFile(tablePath.string(),10000);
            table.second->importFromBinaryFile(tablePath.string());
            std::cout << get_timestamp() << " done sucessfully! "  << std::endl;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }
}
