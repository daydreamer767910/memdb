#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <set>
#include <fstream>
#include <nlohmann/json.hpp>
#include <uv.h>
#include "memtable.hpp"
#include "memdatabase.hpp"
#include "tcpserver.hpp"
#include "logger.hpp"

int main() {
    auto& logger = Logger::get_instance();
    // 输出到文件
    logger.set_log_file("./log/mdbsrv.log");
    // 启用/禁用控制台输出
    logger.enable_console_output(true);
    logger.start();
    logger.log(Logger::LogLevel::INFO, "mdbsrv started ");


    MemDatabase::ptr& db = MemDatabase::getInstance();
    // Create table from JSON
    std::string jsonConfig = R"({
        "name": "users-1",
        "columns": [
            { "name": "id", "type": "int", "primaryKey": true },
            { "name": "name", "type": "string" },
            { "name": "age", "type": "int", "defaultValue": 20 },
            { "name": "addr", "type": "string", "defaultValue": "xxxx" },
            { "name": "created_at", "type": "date" }
        ]
    })";
    db->addTableFromJson(jsonConfig);
    db->addTableFromFile("./etc/table.json");
    auto users = db->getTable("users");
    auto users1 = db->getTable("users-1");

    std::string jsonRows = R"({"rows":[
        {"id": 1, "name": "Bill", "age": 30, "addr": "2651 library lane"},
        {"id": 2, "name": "Emily", "age": 25, "created_at": "2020-12-27 14:45:30"},
        {"id": 3, "name": "Tina", "age": 35, "addr": "742 evergreen terrace"}
    ]})";   
    users1->insertRowsFromJson(jsonRows);
    users->importRowsFromFile("./etc/data.json");

    // Create index
    users->addIndex("name");
    users->showIndexs();
    // Query by index
    Field userName = std::string("Alice");
    const auto& queryRows = users->queryByIndex("name", userName);
    std::cout << "query Alice:" << users->rowsToJson(queryRows).dump(4) << std::endl;
    
    users->showTable();
    users->showRows();
    users1->showTable();
    users1->showRows();
    // Save to JSON file
    users1->exportToFile("./data/users-1.dat");
    users->exportToFile("./data/users.dat");


    // 启动 TCP 服务器
    auto server = std::make_shared<TcpServer>("0.0.0.0", 7899);
    server->start();
    

    return 0;
}
