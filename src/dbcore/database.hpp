#ifndef DATABASE_HPP
#define DATABASE_HPP
//#include <malloc.h>
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

    DataContainer::ptr addContainer(const std::string& name, const std::string& type);
    void addContainer(const json& j);
    void uploadContainer(const std::string& filePath, const std::string& name);
    void saveContainer(const std::string& filePath, const std::string& name);
    DataContainer::ptr getContainer(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = containers_.find(name);
        if (it != containers_.end()) {
            return it->second;
        }
        return nullptr;
    }

    void addContainerFromSchema(const std::string& filePath);

    // 删除容器
    void removeContainer(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = containers_.find(name);
        if (it != containers_.end()) {
            containers_.erase(it);
            //malloc_trim(0);
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
