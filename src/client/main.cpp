#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <random>
#include "net/transportsrv.hpp"
#include "log/logger.hpp"
#include "util/util.hpp"
#include "util/timer.hpp"
//#include "dbcore/field.hpp"
#include "mdbclient.hpp"

extern "C" {
    void mdb_init(const char* user,const char* pwd);
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
static int offset = 0;
int msgid = 1;

int test(const std::string jsonConfig) {
    // 写操作，支持超时
    int ret = 0;
    
    ret = mdb_send(jsonConfig.c_str(),msgid++, 1000);
    if (ret<0) {
        std::cerr << "Write operation failed:" << ret << std::endl;
        if (ret == -2) {
            mdb_reconnect(ip,port);
            return 0;
        }
        return ret;
    }
    if (ret>0) {
        //printf("APP SEND[%d]:\r\n",ret);
        //print_packet(reinterpret_cast<const uint8_t*>(buffer),ret);
        json jsonData = json::parse(jsonConfig.c_str());
        std::cout << jsonData.dump(2) << std::endl;
    }
    
    // 读操作，支持超时
    char buffer[1024*500] = {};
    ret = mdb_recv(buffer, sizeof(buffer) , 3000);
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

void createidx(std::string& name) {
    // 示例 JSON
    std::string json_str = R"({
        "indexes": [
            "_id",
            "id",
            "nested.details.age"
        ]
    })";
    
    // 解析 JSON
    json jsonData = json::parse(json_str);
    //jsonData["fields"] = json::array({ "nested.details.author", "nested.value" });
    jsonData["action"] = "create_idx";
    jsonData["name"] = name;


    // Convert JSON to string
    std::string jsonConfig = jsonData.dump(1);
    
    // Test or use the generated JSON
    test(jsonConfig);
}

void dropidx(std::string& name) {
    // 示例 JSON
    std::string json_str = R"({
        "indexes": [
            "_id",
            "id",
            "nested.details.age"
        ]
    })";
    
    // 解析 JSON
    json jsonData = json::parse(json_str);
    //jsonData["fields"] = json::array({ "nested.details.author", "nested.value" });
    jsonData["action"] = "drop_idx";
    jsonData["name"] = name;

    // Convert JSON to string
    std::string jsonConfig = jsonData.dump(1);
    
    // Test or use the generated JSON
    test(jsonConfig);
}

void delete_collection(std::string& name) {
    // 示例 JSON
    std::string json_str = R"({
        "conditions": [
            {
                "path": "nested.details.age",
                "op": "==",
                "value": 30
            }
        ],
        "fields": [
            "nested.details.age"
        ]
    })";
    
    // 解析 JSON
    json jsonData = json::parse(json_str);
    //jsonData["fields"] = json::array({ "nested.details.author", "nested.value" });
    jsonData["action"] = "delete";
    jsonData["name"] = name;


    // Convert JSON to string
    std::string jsonConfig = jsonData.dump(1);
    
    // Test or use the generated JSON
    test(jsonConfig);
}

void update_collection(std::string& name) {
    // 示例 JSON
    std::string json_str = R"({
        "conditions": [
            {
                "path": "nested.details.age",
                "op": "<",
                "value": 25
            }
        ]
    })";
    
    // 解析 JSON
    json jsonData = json::parse(json_str);
    json updateFields = {
        {"nested.details.author", "nobody"},
        {"nested.details.age", 30},
        {"nested.details.email", "oumss@ou.mass"}
    };
    jsonData["fields"] = updateFields;
    jsonData["action"] = "update";
    jsonData["name"] = name;


    // Convert JSON to string
    std::string jsonConfig = jsonData.dump(1);
    
    // Test or use the generated JSON
    test(jsonConfig);
}
void get_collection(std::string& name) {
    int limit = 10; 
    std::string json_str = R"({
        "conditions": [
            {
                "path": "_id",
                "op": "<",
                "value": 5000
            },
            {
                "path": "nested.details.age",
                "op": ">",
                "value": 22
            }
        ],
        "sorting": {
            "path": "_id",
            "ascending": true
        }
    })";
    
    // 解析 JSON
    json jsonData = json::parse(json_str);
    // 添加分页信息
    jsonData["pagination"] = {
        {"offset", offset},
        {"limit", limit}
    };
    offset += limit;
    jsonData["action"] = "select";
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
                "path": "id",
                "op": "<",
                "value": 100
            },
            {
                "path": "nested.details.created_at",
                "op": ">",
                "value": "${2025-01-01 00:00:00}"
            }
        ],
        "sorting": {
            "path": "nested.details.age",
            "ascending": false
        },
        "pagination": {
            "offset": 0,
            "limit": 10
        },
        "fields": [
            "id",
            "nested.details"
        ]
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
    // 将字符串解析为 JSON 对象
    //json j = json::parse(rawJson);
    json j = json::array();
    for (int k=0; k< 30; ++k) {
        // 生成随机数
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 9999);  // 随机数范围
        int randomValue = dis(gen);

        j.push_back({
            {"id", id++},
            {"_id", id},
            {"title", "cmass"},
            {"version", 1},
            {"nested", {
                {"title", "neseted Document " + std::to_string(id)},
                {"value", randomValue*(id+3)},
                {"details", {
                    {"created_at", "${" + get_timestamp_sec() + "}"},
                    {"author", "anybody " + std::to_string(id)},
                    {"age", id%99+16},
                    {"email", std::to_string(id) + "xx@yy.zz"},
                    {"phone", "+10 123 456-" + std::to_string(randomValue)},
                    {"password", "Sb123456#"},
                    {"stats", {
                        {"views", randomValue*id},
                        {"likes", randomValue/(id+1)},
                        {"shares", randomValue*(id+10)}
                    }}
                }}
            }}
        });
    }
    
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
        "schema": {
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
    j["action"] = "create";
    j["name"] = name;
    j["type"] = "collection";

    // Convert JSON to string
    std::string jsonConfig = j.dump(1);
    
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
        row["created_at"] = "${" + get_timestamp_sec() + "}";
        jsonRows.push_back(row);
        id++;
    }
    jsonData["rows"] = jsonRows;

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
    int limit = 20;
    json jsonData;
    std::vector<std::string> columnNames = {"id", "name", "age", "created_at"};
    std::vector<std::string> conditions = {"id", "created_at"};
    std::vector<FieldValue> queryValues = {25, std::string("${2025-01-19 00:16:30}")};
    std::vector<std::string> operators = {">=", ">"};
    jsonData["action"] = "select";
    jsonData["name"] = name;
    jsonData["limit"] = limit;
    jsonData["offset"] = offset;
    offset += limit;
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
    std::string passwd = "11111111";
    
    mdb_init("memdb",passwd.c_str());
    
    // 如果数据库连接失败，则每隔 3 秒重试
    while (mdb_start(ip, port) < 0) {
        sleep(3);
        if (exiting) {
            return -1;
        }
    }
    //Ecdh("memdb", "fuckyou");
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
    } else if (command == "create_idx") {
        createidx(param);
    } else if (command == "drop_idx") {
        dropidx(param);
    } else if (command == "deletec") {
        delete_collection(param);
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
        while (!exiting) {
            insert_collection(param);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    } else if (command == "insert") {
        while (!exiting) {
            insert_tbl(param);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } else if (command == "getc") {
        while (!exiting) {
            get_collection(param);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } else if (command == "get") {
        while (!exiting) {
            select(param);
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
