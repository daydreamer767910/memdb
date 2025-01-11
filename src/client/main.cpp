#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "net/transportsrv.hpp"
#include "log/logger.hpp"
#include "util/util.hpp"
#include "util/timer.hpp"
#include "tcpclient.hpp"
#include "mdbclient.hpp"

boost::asio::io_context io_context;

// 创建 TcpClient 对象
std::string host = "127.0.0.1";
std::string port = "7899";
auto client_ptr = MdbClient::create(io_context, host, port);

int init() {
    // 连接到服务器
    MdbClient::start(client_ptr);
    return 0;
}

int test(const std::string jsonConfig) {
    // 写操作，支持超时
    
    json jsonData = nlohmann::json::parse(jsonConfig);
    if (client_ptr->send(jsonData,1, 1000)<0) {
        std::cerr << "Write operation failed." << std::endl;
        return 1;
    }
    // 读操作，支持超时
    std::vector<json> jsonDatas;
    if (client_ptr->recv(jsonDatas, 2000)<0) {
        std::cerr << "Read operation failed." << std::endl;
        return 1;
    }

    for (auto recvJson : jsonDatas) {
        printf("APP RECV[%d]:\r\n",jsonDatas.size());
        std::cout << recvJson.dump(4) << std::endl;
    }
    return 0;
    
}

void create_tbl() {
//1.create table
    std::string jsonConfig = R"({
        "action": "create table",
        "name": "client-test",
        "columns": [
            { "name": "id", "type": "int", "primaryKey": true },
            { "name": "name", "type": "string" },
            { "name": "age", "type": "int", "defaultValue": 20 },
            { "name": "addr", "type": "string", "defaultValue": "xxxx" },
            { "name": "created_at", "type": "date" }
        ]
    })";
    
    test(jsonConfig);
}

void show_tbl() {
//2.show tables
    std::string jsonConfig = R"({
        "action": "show tables"
    })";
    
    test(jsonConfig);
}

void insert_tbl() {
//3.insert rows
    json jsonData;
    jsonData["action"] = "insert";
    jsonData["name"] = "client-test";
    json jsonRows = json::array();
    for (int i=0;i<20;i++) {
        json row;
        row["id"] = i;
        row["name"] = "test name" + std::to_string(i);
        row["age"] = 20+i;
        row["addr"] = "street " + std::to_string(i);
        jsonRows.push_back(row);
    }
    jsonData["rows"] = jsonRows;

    std::string jsonConfig = jsonData.dump(1);
    test(jsonConfig);
}

void get() {
    std::string jsonConfig = R"({
        "action": "get",
        "name": "client-test"
    })";
    test(jsonConfig);
}



int main(int argc, char* argv[]) {
    std::thread mdbclient(init);
    sleep(2);
    create_tbl();
    show_tbl();
    insert_tbl();
    while(true) {
        get();
        sleep(10);
    }
    if (mdbclient.joinable()) {
        mdbclient.join();
    }
    // 关闭连接
    client_ptr->stop();
    
    return 0;
}
