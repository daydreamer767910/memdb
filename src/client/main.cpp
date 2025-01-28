#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "net/transportsrv.hpp"
#include "log/logger.hpp"
#include "util/util.hpp"
#include "util/timer.hpp"
#include "dbcore/field.hpp"
#include "mdbclient.hpp"

extern "C" {
    void mdb_init();
    void mdb_stop();
    void mdb_quit();
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

void update_collection(std::string& name) {
    // 示例 JSON
    std::string json_str = R"({
        "conditions": [
            {
                "path": "nested.details.age",
                "op": ">",
                "value": 25
            },
            {
                "path": "nested.details.email",
                "op": "regex",
                "value": "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$"
            }
        ]
    })";
    
    // 解析 JSON
    json jsonData = json::parse(json_str);
    json updateFields = {
        {"nested.details.author", "nobody"},
        {"nested.details.age", 30},
        {"nested.details.newone", "for test"}
    };
    jsonData["fields"] = updateFields;
    jsonData["action"] = "update";
    jsonData["name"] = name;


    // Convert JSON to string
    std::string jsonConfig = jsonData.dump(1);
    
    // Test or use the generated JSON
    test(jsonConfig);
}

void query_collection(std::string& name) {
    // 示例 JSON
    std::string json_str = R"({
        "conditions": [
            {
                "path": "nested.details.age",
                "op": ">",
                "value": 25
            },
            {
                "path": "nested.details.email",
                "op": "regex",
                "value": "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$"
            }
        ],
        "sorting": {
            "path": "nested.details.age",
            "ascending": true
        },
        "pagination": {
            "offset": 0,
            "limit": 10
        }
    })";
    
    // 解析 JSON
    json jsonData = json::parse(json_str);

    jsonData["action"] = "select";
    jsonData["name"] = name;


    // Convert JSON to string
    std::string jsonConfig = jsonData.dump(1);
    
    // Test or use the generated JSON
    test(jsonConfig);
}

void insert_collection(std::string& name) {
    std::string rawJson = R"([{
        "id": 3,
        "title": "cmass",
        "version": 1,
        "nested": {
            "title": "1 Document",
            "value": 43,
            "details": {
                "created_at": "2025-01-24",
                "author": "Emily Doe",
                "age": 27,
                "email": "xx@yy.zz",
                "phone": "+10 123 456-7890",
                "password": "Sb123456#",
                "stats": {
                    "views": 1500,
                    "likes": 3005,
                    "shares": 750
                }
            }
        }
    },
    {
        "id": 1,
        "title": "ou mass",
        "nested": {
            "title": "2 Document",
            "details": {
            "author": "Emily LEE",
                "age": 26,
                "stats": {
                    "views": 15000,
                    "shares": 750
                }
            }
        }
    }])";
    // 将字符串解析为 JSON 对象
    json j = json::parse(rawJson);
    json jsonData;
    jsonData["action"] = "insert";
    jsonData["name"] = name;
    jsonData["documents"] = j;

    // Convert JSON to string
    std::string jsonConfig = jsonData.dump(1);
    
    // Test or use the generated JSON
    test(jsonConfig);
}

void create_collection(std::string& name) {
/*
email限制:
    ^[a-zA-Z0-9._%+-]+：用户名部分，可以包含字母、数字、点号、下划线和破折号
    @[a-zA-Z0-9.-]+：@ 后的域名部分
    \.[a-zA-Z]{2,}$：域名后缀，至少 2 个字母

密码限制:
    (?=.*[a-z])：至少一个小写字母
    (?=.*[A-Z])：至少一个大写字母
    (?=.*\d)：至少一个数字
    (?=.*[#@$!%*?&])：至少一个特殊字符
    [A-Za-z\d@$!%*?&]{8,}：至少 8 个字符，且由这些字符组成

电话限制:
    ^\+?：可选的 +，表示国际电话
    [1-9]\d{0,2}：国家代码（1-3 位数字）
    [-\s]?：可选的破折号或空格
    (\d{3}[-\s]?){2}：中间的 3 位区号和号码部分，可用空格或破折号分隔
    \d{4}$：最后的 4 位号码
*/
    // 原始 JSON 数据
    std::string rawJson = R"({
        "fields": {
            "id": {
                "type": "int",
                "constraints": {
                    "required": true,
                    "depth": 1
                }
            },
            "title": {
                "type": "string",
                "constraints": {
                    "depth": 3,
                    "required": true,
                    "minLength": 5,
                    "maxLength": 50
                }
            },
            "email": {
                "type": "string",
                "constraints": {
                    "required": false,
                    "minLength": 5,
                    "maxLength": 100,
                    "regexPattern": "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$"
                }
            },
            "phone": {
                "type": "string",
                "constraints": {
                    "required": false,
                    "minLength": 8,
                    "maxLength": 16,
                    "regexPattern": "^\\+?[1-9]\\d{0,2}[-\\s]?(\\d{3}[-\\s]?){2}\\d{4}$"
                }
            },
            "password": {
                "type": "string",
                "constraints": {
                    "required": false,
                    "minLength": 8,
                    "maxLength": 16,
                    "regexPattern": "^(?=.*[a-z])(?=.*[A-Z])(?=.*\\d)(?=.*[#@$!%*?&])[A-Za-z\\d#@$!%*?&]{8,}$"
                }
            },
            "age":{
                "type": "int",
                "constraints": {
                    "required": false,
                    "minValue": 1,
                    "maxValue": 120
                }
            },
            "nested": {
                "type": "document",
                "constraints": {
                    "depth": 1,
                    "required": true
                }
            },
            "details": {
                "type": "document",
                "constraints": {
                    "depth": 2,
                    "required": true
                }
            }
        }
    })";

    // 将字符串解析为 JSON 对象
    json j = json::parse(rawJson);
    json jsonData;
    jsonData["action"] = "create";
    jsonData["name"] = name;
    jsonData["type"] = "collection";
    jsonData["schema"] = j;
    // Convert JSON to string
    std::string jsonConfig = jsonData.dump(1);
    
    // Test or use the generated JSON
    test(jsonConfig);
}

void create_tbl(std::string& name) {
    // Create table action
    json jsonData;
    jsonData["action"] = "create";
    jsonData["name"] = name;
    jsonData["type"] = "table";
    
    // Create columns array
    json jsonCols = json::array();

    // Column definitions
    jsonCols.push_back({ 
        {"name", "id"}, 
        {"type", "int"}, 
        {"nullable", false},
        {"primaryKey", true} 
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
        {"name", "data"}, 
        {"type", "binary"}, 
        {"nullable", false},
        //{"defaultValue", "xxxx"} 
    });

    jsonCols.push_back({ 
        {"name", "created_at"}, 
        {"type", "time"},
        {"indexed", true}
    });

    // Attach columns to the main data
    jsonData["columns"] = jsonCols;

    // Convert JSON to string
    std::string jsonConfig = jsonData.dump(1);
    
    // Test or use the generated JSON
    test(jsonConfig);
}

void drop_tbl(std::string& name) {
    json jsonData;
    jsonData["action"] = "drop";
    jsonData["name"] = name;
    std::string jsonConfig = jsonData.dump(1);
    test(jsonConfig);
}

void show_tbl(std::string& name) {
    json jsonData;
    jsonData["action"] = "show";
    jsonData["name"] = name;
    std::string jsonConfig = jsonData.dump(1);
    test(jsonConfig);
}


void insert_tbl(std::string& name) {
//3.insert rows
    json jsonData;
    jsonData["action"] = "insert";
    jsonData["name"] = name;
    json jsonRows = json::array();
    
    for (int i=0;i<50;i++) {
        json row;
        row["id"] = id;
        row["name"] = "test name" + std::to_string(id);
        row["age"] = 20+i%50;
        row["addr"] = "street " + std::to_string(id);
        std::vector<uint8_t> binaryData = { 55, 20, 108, 2, 1, 100, 200, 22, 17, 84, 23, 9}; 
        row["data"] = binaryData;
        //row["created_at"] = "${2024-12-22 00:00:00}";
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

void update(std::string& name) {
    json jsonData;
    std::vector<std::string> columnNames = {"name", "age","created_at"};
    std::vector<FieldValue> newValues = {std::string("nobody"), 25,std::string("${2025-01-18 19:16:30}")};  // 新值
    std::vector<std::string> conditions = {"age", "created_at"};
    std::vector<FieldValue> queryValues = {25, std::string("${2025-01-19 19:16:30}")};
    std::vector<std::string> operators = {">=", "<"};
    jsonData["action"] = "update";
    jsonData["name"] = name;
    jsonData["columns"] = columnNames;
    jsonData["conditions"] = conditions;
    json jsonArray = json::array();
    for (const auto& value : newValues) {
        jsonArray.push_back(Field(value).toJson());
    }
    jsonData["values"] = jsonArray;
    jsonData["ops"] = operators;
    jsonArray = json::array();
    for (const auto& value : queryValues) {
        jsonArray.push_back(Field(value).toJson());
    }
    jsonData["qvalues"] = jsonArray;
    std::string jsonConfig = jsonData.dump(1);
    test(jsonConfig);
}


void select(std::string& name) {
    json jsonData;
    std::vector<std::string> columnNames = {"id", "name", "age", "created_at"};
    std::vector<std::string> conditions = {"age", "created_at"};
    std::vector<FieldValue> queryValues = {25, std::string("${2025-01-19 00:16:30}")};
    std::vector<std::string> operators = {"==", "<"};
    jsonData["action"] = "select";
    jsonData["name"] = name;
    jsonData["limit"] = 5000;
    jsonData["columns"] = columnNames;
    jsonData["conditions"] = conditions;
    jsonData["ops"] = operators;
    json jsonArray = json::array();
    for (const auto& value : queryValues) {
        jsonArray.push_back(Field(value).toJson());
    }
    jsonData["qvalues"] = jsonArray;
    std::string jsonConfig = jsonData.dump(1);
    test(jsonConfig);
}

void remove(std::string& name) {
    json jsonData;
    std::vector<std::string> conditions = {"age", "created_at"};
    std::vector<FieldValue> queryValues = {25, std::string("${2025-01-18 00:16:30}")};
    std::vector<std::string> operators = {">=", ">="};
    jsonData["action"] = "delete";
    jsonData["name"] = name;
    jsonData["conditions"] = conditions;
    jsonData["ops"] = operators;
    json jsonArray = json::array();
    for (const auto& value : queryValues) {
        jsonArray.push_back(Field(value).toJson());
    }
    jsonData["qvalues"] = jsonArray;
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
    } else if (command == "createc") {
        create_collection(param);
    }else if (command == "drop") {
        drop_tbl(param);
    } else if (command == "show") {
        show_tbl(param);
    } else if (command == "count") {
        count(param);
    } else if (command == "delete") {
        remove(param);
    } else if (command == "select") {
        select(param);
    } else if (command == "selectc") {
        query_collection(param);
    } else if (command == "updatec") {
        update_collection(param);
    } else if (command == "update") {
        update(param);
    } else if (command == "insertc") {
        insert_collection(param);
    } else if (command == "insert") {
        while (!exiting) {
            insert_tbl(param);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
       //insert_collection(param);
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
    mdb_quit();
    return 0;
}
