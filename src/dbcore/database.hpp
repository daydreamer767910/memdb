#ifndef DATABASE_HPP
#define DATABASE_HPP

#include "datacontainer.hpp"
#include "table.hpp"
#include "collection.hpp"
class Database {
private:
    std::unordered_map<std::string, DataContainer::ptr> containers_;
    mutable std::mutex mutex_;

private:
    Database() {
        std::cout << "database created!" << std::endl;
    }

    // 禁止拷贝构造和赋值操作
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
public:
    using ptr = std::shared_ptr<Database>;

    // 获取单例实例
    static ptr& getInstance() {
        static ptr instance(new Database());
        return instance;
    }

    void addContainer(const json& j);

    DataContainer::ptr getContainer(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = containers_.find(name);
        if (it != containers_.end()) {
            return it->second;
        }
        throw std::runtime_error("Container not found");
    }

    void addContainerFromSchema(const std::string& filePath);

    Table::ptr getTable(const std::string& tableName);
    // 删除容器
    void removeContainer(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = containers_.find(name);
        if (it != containers_.end()) {
            containers_.erase(it);
        } else {
            throw std::runtime_error("Container not found: " + name);
        }
    }


    std::vector<DataContainer::ptr> listContainers() const;

    void save(const std::string& filePath);
    void upload(const std::string& filePath);
    void remove(const std::string& filePath, const std::string& name);
};


#endif // database_H
