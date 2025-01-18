#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "net/transportsrv.hpp"
#include "log/logger.hpp"
#include "util/util.hpp"
#include "util/timer.hpp"

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
static int id = 0;

int test(const std::string jsonConfig) {
    // 写操作，支持超时
    int ret = 0;
    
    ret = mdb_send(jsonConfig.c_str(),1, 1000);
    if (ret<0) {
        std::cerr << "Write operation failed:" << ret << std::endl;
        if (ret == -2) {
            mdb_reconnect(ip,port);
            return 0;
        }
        return ret;
    }
    
    // 读操作，支持超时
    char buffer[1024*500] = {};
    ret = mdb_recv(buffer, sizeof(buffer) , 1000);
    if (ret<0) {
        std::cerr << "Read operation failed:" << ret << std::endl;
        if (ret == -2) {
            mdb_reconnect(ip,port);
            return 0;
        }
        return ret;
    }

    if (ret>0) {
        //printf("APP RECV[%d]:\r\n",ret);
        //print_packet(reinterpret_cast<const uint8_t*>(buffer),ret);
        json jsonData = json::parse(buffer);
        std::cout << jsonData.dump(2) << std::endl;
    }
    return ret;
    
}

void create_tbl(std::string& name) {
    // Create table action
    json jsonData;
    jsonData["action"] = "create";
    jsonData["name"] = name;
    
    // Create columns array
    json jsonCols = json::array();

    // Column definitions
    jsonCols.push_back({ 
        {"name", "id"}, 
        {"type", "int"}, 
        {"nullable", true},
        {"defaultValue", 0}
       // {"primaryKey", true} 
    });

    jsonCols.push_back({ 
        {"name", "name"}, 
        {"type", "string"},
        {"nullable", true},
        {"defaultValue", "sb"}
    });

    jsonCols.push_back({ 
        {"name", "age"}, 
        {"type", "int"}, 
        //{"defaultValue", 20} 
    });

    jsonCols.push_back({ 
        {"name", "addr"}, 
        {"type", "string"}, 
        {"nullable", true},
        {"defaultValue", "xxxx"} 
    });

    jsonCols.push_back({ 
        {"name", "created_at"}, 
        {"type", "date"} 
    });

    // Attach columns to the main data
    jsonData["columns"] = jsonCols;

    // Convert JSON to string
    std::string jsonConfig = jsonData.dump(1);
    
    // Test or use the generated JSON
    test(jsonConfig);
}

void show_tbl() {
//2.show tables
    std::string jsonConfig = R"({
        "action": "show"
    })";
    
    test(jsonConfig);
}

void insert_tbl(std::string& name) {
//3.insert rows
    json jsonData;
    jsonData["action"] = "insert";
    jsonData["name"] = name;
    json jsonRows = json::array();

    for (int i=0;i<30;i++) {
        json row;
        row["id"] = id;
        //row["name"] = "test name" + std::to_string(id);
        row["age"] = 20+i%50;
        row["addr"] = "street " + std::to_string(id);
        jsonRows.push_back(row);
        id++;
    }
    jsonData["rows"] = jsonRows;

    std::string jsonConfig = jsonData.dump(1);
    test(jsonConfig);
}

void get(std::string& name) {
    static int offset = 0;
    int limit = 30;
    json jsonData;
    jsonData["action"] = "get";
    jsonData["name"] = name;
    jsonData["limit"] = limit;
    jsonData["offset"] = offset;
    offset += limit;
    std::string jsonConfig = jsonData.dump(1);
    test(jsonConfig);
}

void count(std::string& name) {
    json jsonData;
    jsonData["action"] = "count";
    jsonData["name"] = name;

    std::string jsonConfig = jsonData.dump(1);
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
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <command>\n";
        std::cerr << "Commands: create, show, insert, get, count\n";
        return 1;
    }

    // 设置信号处理程序
    std::signal(SIGINT, signal_handler);
    
    mdb_init();
    
    // 如果数据库连接失败，则每隔 3 秒重试
    while (mdb_start(ip, port) < 0) {
        sleep(3);
        if (exiting) {
            break;
        }
    }

    // 根据命令行参数执行相应的操作
    std::string command = argv[1];
    std::string param = argv[2];
    
    if (command == "create") {
        create_tbl(param);
    } else if (command == "show") {
        show_tbl();
    } else if (command == "count") {
        count(param);
    } else if (command == "insert") {
        while (!exiting) {
            insert_tbl(param);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } else if (command == "get") {
        while (!exiting) {
            get(param);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } else {
        std::cerr << "Invalid command. Valid commands are: create, show, insert, get, count.\n";
    }
    
    // 关闭连接
    mdb_stop();

    return 0;
}
