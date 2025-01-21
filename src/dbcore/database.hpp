#ifndef DATABASE_HPP
#define DATABASE_HPP

#include "table.hpp"

class Database {
private:
    std::map<std::string, Table::ptr> tables;

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


    void addTableFromFile(const std::string& filePath);
    void saveTableToFile(Table::ptr table, const std::string& filePath);

    // Methods to manipulate tables
    void addTableFromJson(const std::string& jsonConfig);
    void addTable(const std::string& tableName, const std::vector<Column>& columns);
    Table::ptr getTable(const std::string& tableName);
    //void updateTable(Table&& updatedTable);
    void removeTable(const std::string& tableName);

    // Methods for listing tables
    std::vector<std::string> listTableNames() const;
    std::vector<Table::ptr> listTables() const;

    void save(const std::string& filePath);
    void upload(const std::string& filePath);
    void remove(const std::string& filePath, const std::string& tableName);
};


#endif // database_H
