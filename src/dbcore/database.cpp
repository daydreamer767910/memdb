#include <iostream>
#include <filesystem>
#include "database.hpp"

// Add a new table to the database
void Database::addTable(const std::string& tableName, const std::vector<Column>& columns) {
    if (tables.find(tableName) != tables.end()) {
        throw std::invalid_argument("Table " + tableName + " already exists.");
    }
    tables.emplace(tableName, std::make_shared<Table>(tableName, columns));
}

void Database::addTableFromJson(const std::string& jsonConfig) {
    auto root = json::parse(jsonConfig);
    //std::cout << root.dump(4) << std::endl;
    std::string tableName = root["name"];
    auto columns = jsonToColumns(root);
    addTable(tableName, columns);
}

void Database::addTableFromFile(const std::string& filePath) {
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
Table::ptr Database::getTable(const std::string& tableName) {
    auto it = tables.find(tableName);
    return it != tables.end() ? it->second : nullptr;
}

// Update an existing table
/*void Database::updateTable(Table&& updatedTable) {
    auto it = tables.find(updatedTable.name);
    if (it == tables.end()) {
        throw std::invalid_argument("Table " + updatedTable.name + " does not exist.");
    }
    it->second = std::make_shared<Table>(std::move(updatedTable));  // Use move semantics here
}*/

// Remove a table by its name
void Database::removeTable(const std::string& tableName) {
    auto it = tables.find(tableName);
    if (it == tables.end()) {
        throw std::invalid_argument("Table " + tableName + " does not exist.");
    }
    tables.erase(it);
}

// List all table names in the Database
std::vector<std::string> Database::listTableNames() const {
    std::vector<std::string> tableNames;
    for (const auto& table : tables) {
        tableNames.push_back(table.first);
    }
    return tableNames;
}

// Get all table instances in the Database
std::vector<Table::ptr> Database::listTables() const {
    std::vector<Table::ptr> tableInstances;
    for (const auto& table : tables) {
        tableInstances.push_back(table.second);
    }
    return tableInstances;
}

void Database::saveTableToFile(Table::ptr table, const std::string& filePath) {
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


void Database::save(const std::string& filePath) {
    if (tables.empty()) {
        std::cerr << "Warning: No tables to save.\n";
        return;
    }

    try {
        // 保存配置文件
        std::string subDir = "config";
        std::filesystem::path configPath = std::filesystem::path(filePath) / subDir;
        std::filesystem::create_directories(configPath);
        for (const auto& table : tables) {
            std::filesystem::path tableConfigPath = configPath / table.first;
            try {
                saveTableToFile(table.second, tableConfigPath.string());
                std::cout << get_timestamp() << " Saved config for table: " << table.first << '\n';
            } catch (const std::exception& e) {
                std::cerr << get_timestamp() << " Failed to save config for table " << table.first 
                          << ": " << e.what() << '\n';
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error creating config directory: " << e.what() << '\n';
    }

    try {
        // 保存数据文件
        std::string subDir = "data";
        std::filesystem::path dataPath = std::filesystem::path(filePath) / subDir;
        std::filesystem::create_directories(dataPath);
        for (const auto& table : tables) {
            std::filesystem::path tableDataPath = dataPath / table.first;
            try {
                std::cout << get_timestamp() << " Start saving table data: " << table.first << '\n';
                table.second->exportToBinaryFile(tableDataPath.string());
                std::cout << get_timestamp() << " Successfully saved table data: " << table.first << '\n';
            } catch (const std::exception& e) {
                std::cerr << get_timestamp() << " Failed to save data for table " << table.first 
                          << ": " << e.what() << '\n';
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error creating data directory: " << e.what() << '\n';
    }
}


void Database::upload(const std::string& filePath) {
    try {
        // 处理 config 目录
        std::string subDir = "config";
        std::filesystem::path fullPath = std::filesystem::path(filePath) / subDir;
        if (!std::filesystem::exists(fullPath)) {
            std::cerr << "Warning: Config directory " << fullPath << " does not exist.\n";
        } else if (!std::filesystem::is_empty(fullPath)) {
            for (const auto& entry : std::filesystem::directory_iterator(fullPath)) {
                if (entry.is_regular_file()) {
                    this->addTableFromFile(entry.path().string());
                }
            }
        }

        // 处理 data 目录
        subDir = "data";
        fullPath = std::filesystem::path(filePath) / subDir;
        if (!std::filesystem::exists(fullPath)) {
            std::cerr << "Warning: Data directory " << fullPath << " does not exist.\n";
        } else if (!tables.empty()) {
            for (const auto& table : tables) {
                std::filesystem::path tablePath = std::filesystem::path(fullPath) / table.first;
                try {
                    std::cout << get_timestamp() << " Loading table: " << table.first << '\n';
                    table.second->importFromBinaryFile(tablePath.string());
                    std::cout << get_timestamp() << " Table loaded successfully.\n";
                } catch (const std::exception& e) {
                    std::cerr << get_timestamp() << " Failed to load table " << table.first 
                              << " from " << tablePath << ": " << e.what() << '\n';
                }
            }
        } else {
            std::cerr << "Warning: No tables available for data import.\n";
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Standard exception: " << e.what() << '\n';
    } catch (...) {
        std::cerr << "Unknown error occurred while uploading files.\n";
    }
}

void Database::remove(const std::string& filePath, const std::string& tableName) {
    try {
        std::vector<std::string> subDirs = {"config", "data"};
        for (const auto& subDir : subDirs) {
            std::filesystem::path fullPath = std::filesystem::path(filePath) / subDir / tableName;
            if (std::filesystem::exists(fullPath)) {
                if (std::filesystem::is_regular_file(fullPath)) {
                    std::filesystem::remove(fullPath);
                    std::cout << "Removed file: " << fullPath << '\n';
                } else {
                    std::cerr << "Error: Path " << fullPath << " is not a regular file.\n";
                }
            } else {
                std::cerr << "Warning: File " << fullPath << " does not exist.\n";
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Standard exception: " << e.what() << '\n';
    } catch (...) {
        std::cerr << "Unknown error occurred while removing files.\n";
    }
}
