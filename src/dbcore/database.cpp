#include <iostream>
#include <filesystem>
#include "database.hpp"

DataContainer::ptr Database::addContainer(const std::string& name, const std::string& type) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = containers_.find(name);
    if (it != containers_.end()) { // 如果已存在，直接返回
        std::cerr << "Container [" << name << "] already exists\n";
        return it->second;
    }

    // 创建 container
    DataContainer::ptr container;
    if (type == "table") {
        container = std::make_shared<Table>(name, type);
    } else if (type == "collection") {
        container = std::make_shared<Collection>(name, type);
    } else {
        throw std::invalid_argument("Unknown container type: " + type);
    }

    // 插入 map 并返回
    return containers_.emplace(name, container).first->second;
}

void Database::addContainer(const json& j) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string type = "table";
    std::string name = j.at("name").get<std::string>();
    if (j.contains("type")) {
        type = j.at("type").get<std::string>();
    }

    if (containers_.find(name) != containers_.end()) {
        throw std::invalid_argument("container " + name + " already exists.");
    }
    DataContainer::ptr container;
    if (type == "table") {
        container = std::make_shared<Table>(name,type);  
    } else if (type == "collection") {
        container = std::make_shared<Collection>(name,type);
    } else {
        throw std::invalid_argument("Unknown container type");
    }
    container->fromJson(j);
    containers_.emplace(name, container);
}

void Database::addContainerFromSchema(const std::string& filePath) {
    std::ifstream inputFile(filePath);
    if (!inputFile.is_open()) {
        throw std::runtime_error("Unable to open file: " + filePath);
    }
    // 将文件内容读取到std::string中
    std::stringstream buffer;
    buffer << inputFile.rdbuf();
    std::string fileContent = buffer.str();
    auto j = json::parse(fileContent);
    addContainer(j);
    inputFile.close();
}

// Get all container instances in the Database
std::vector<DataContainer::ptr> Database::listContainers() const {
    std::vector<DataContainer::ptr> tableInstances;
    for (const auto& container : containers_) {
        tableInstances.push_back(container.second);
    }
    return tableInstances;
}


void Database::save(const std::string& filePath) {
    if (containers_.empty()) {
        std::cerr << "Warning: No container to save.\n";
        return;
    }

    try {
        // 保存配置文件
        std::string subDir = "config";
        std::filesystem::path configPath = std::filesystem::path(filePath) / subDir;
        std::filesystem::create_directories(configPath);
        for (const auto& container : containers_) {
            std::filesystem::path tableConfigPath = configPath / container.first;
            try {
                container.second->saveSchema(tableConfigPath.string());
                std::cout << get_timestamp() << " Saved config for container: " << container.first << '\n';
            } catch (const std::exception& e) {
                std::cerr << get_timestamp() << " Failed to save config for container: " << container.first 
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
        for (const auto& container : containers_) {
            std::filesystem::path tableDataPath = dataPath / container.first;
            try {
                std::cout << get_timestamp() << " Start saving table data: " << container.first << '\n';
                container.second->exportToBinaryFile(tableDataPath.string());
                std::cout << get_timestamp() << " Successfully saved table data: " << container.first << '\n';
            } catch (const std::exception& e) {
                std::cerr << get_timestamp() << " Failed to save data for table " << container.first 
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
                    this->addContainerFromSchema(entry.path().string());
                }
            }
        }

        // 处理 data 目录
        subDir = "data";
        fullPath = std::filesystem::path(filePath) / subDir;
        if (!std::filesystem::exists(fullPath)) {
            std::cerr << "Warning: Data directory " << fullPath << " does not exist.\n";
        } else if (!containers_.empty()) {
            for (const auto& container : containers_) {
                std::filesystem::path dataPath = std::filesystem::path(fullPath) / container.first;
                try {
                    std::cout << get_timestamp() << " Loading container: " << container.first << '\n';
                    container.second->importFromBinaryFile(dataPath.string());
                    std::cout << get_timestamp() << " container loaded successfully.\n";
                } catch (const std::exception& e) {
                    std::cerr << get_timestamp() << " Failed to load container " << container.first 
                              << " from " << dataPath << ": " << e.what() << '\n';
                }
            }
        } else {
            std::cerr << "Warning: No containers available for data import.\n";
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Standard exception: " << e.what() << '\n';
    } catch (...) {
        std::cerr << "Unknown error occurred while uploading files.\n";
    }
}

void Database::remove(const std::string& filePath, const std::string& name) {
    try {
        std::vector<std::string> subDirs = {"config", "data"};
        for (const auto& subDir : subDirs) {
            std::filesystem::path fullPath = std::filesystem::path(filePath) / subDir / name;
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
