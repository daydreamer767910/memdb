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
    void mdb_init();
    void mdb_stop();
    int mdb_start(const char* ip, int port);
    int mdb_reconnect(const char* ip, int port);
    int mdb_recv(char* buffer, int size, int timeout);
    int mdb_send(const char* json_strdata, int msg_id, int timeout);
}


const char ip[] = "127.0.0.1";
int port = 7900;
bool exiting = false;

int test(const std::string jsonConfig) {
    // 写操作，支持超时
    int ret = 0;
    ret = mdb_send(jsonConfig.c_str(),1, 1000);
    if (ret<0) {
        std::cerr << "Write operation failed." << std::endl;
        if (ret == -2) {
            mdb_reconnect(ip,port);
            return 0;
        }
        return ret;
    }
    // 读操作，支持超时
    char buffer[1024*10] = {};
    ret = mdb_recv(buffer, sizeof(buffer) , 2000);
    if (ret<0) {
        std::cerr << "Read operation failed." << std::endl;
        if (ret == -2) {
            mdb_reconnect(ip,port);
            return 0;
        }
        return ret;
    }

    if (ret>0) {
        printf("APP RECV[%d]:\r\n",ret);
        json jsonData = json::parse(buffer);
        std::cout << jsonData.dump(2) << std::endl;
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
    mdb_init();
    while(mdb_start(ip,port)<0) {
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
    mdb_stop();
EXIT:
    return 0;
}
