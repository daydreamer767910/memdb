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

extern "C" {
    MdbClient::ptr init();
}

MdbClient::ptr client_ptr = init();

bool exiting = false;

int test(const std::string jsonConfig) {
    // 写操作，支持超时
    int ret = 0;
    json jsonData = nlohmann::json::parse(jsonConfig);
    ret = client_ptr->send(jsonData,1, 1000);
    if (ret<0) {
        std::cerr << "Write operation failed." << std::endl;
        if (ret == -2) {
            client_ptr->reconnect("127.0.0.1","7899");
            return 0;
        }
        return ret;
    }
    // 读操作，支持超时
    std::vector<json> jsonDatas;
    ret = client_ptr->recv(jsonDatas, 2000);
    if (ret<0) {
        std::cerr << "Read operation failed." << std::endl;
        if (ret == -2) {
            client_ptr->reconnect("127.0.0.1","7899");
            return 0;
        }
        return ret;
    }

    for (auto recvJson : jsonDatas) {
        printf("APP RECV[%d]:\r\n",jsonDatas.size());
        std::cout << recvJson.dump(4) << std::endl;
    }
    return ret;
    
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

void signal_handler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nSIGINT received. Preparing to exit..." << std::endl;
        //logger.terminate();
        exiting = true;
    }
}

int main(int argc, char* argv[]) {
    // 设置信号处理程序
    std::signal(SIGINT, signal_handler);

    while(client_ptr->start("127.0.0.1","7899")<0) {
        sleep(3);
        if(exiting) goto EXIT;
    }
    
    create_tbl();
    show_tbl();
    insert_tbl();
    while(!exiting) {
        get();
        sleep(3);
    }
    
    // 关闭连接
    client_ptr->stop();
EXIT:
    return 0;
}
