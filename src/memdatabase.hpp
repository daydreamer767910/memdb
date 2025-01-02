#ifndef MEMDATABASE_HPP
#define MEMDATABASE_HPP

#include "memtable.hpp"

class MemDatabase {
private:
    std::map<std::string, MemTable::ptr> tables;

private:
    MemDatabase() {
        std::cout << "MemDatabase created!" << std::endl;
    }

    // 禁止拷贝构造和赋值操作
    MemDatabase(const MemDatabase&) = delete;
    MemDatabase& operator=(const MemDatabase&) = delete;
public:
    using ptr = std::unique_ptr<MemDatabase>;

    // 获取单例实例
    static ptr& getInstance() {
        static ptr instance(new MemDatabase());
        return instance;
    }


    void addTableFromFile(const std::string& filePath);

    // Methods to manipulate tables
    void addTableFromJson(const std::string& jsonConfig);
    void addTable(const std::string& tableName, const std::vector<Column>& columns);
    MemTable::ptr getTable(const std::string& tableName);
    void updateTable(const MemTable& updatedTable);
    void removeTable(const std::string& tableName);

    // Methods for listing tables
    std::vector<std::string> listTableNames() const;
    std::vector<MemTable::ptr> listTables() const;
};


#endif // MEMDATABASE_H
