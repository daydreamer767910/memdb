#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "net/transportsrv.hpp"
#include "log/logger.hpp"
#include "util/util.hpp"
#include "util/timer.hpp"

Logger& logger = Logger::get_instance(); // 定义全局变量

int main() {
    int sock = 0;
    struct sockaddr_in server_address;

    // 创建 socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error\n";
        return -1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(7899);

    // 转换 IP 地址
    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported\n";
        return -1;
    }

    // 连接到服务器
    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        std::cerr << "Connection Failed\n";
        return -1;
    }
//0.app read timer
    auto transport = std::make_shared<Transport>(40096,uv_default_loop());
    Timer timer(uv_default_loop(), 1000, 1000, [&]() {
        std::vector<json> json_datas;
        if(transport->read_all(json_datas,std::chrono::milliseconds(1000)) >0 ) {
            for (auto recvJson : json_datas) {
                printf("APP RECV[%d]:\r\n",json_datas.size());
                std::cout << recvJson.dump(4) << std::endl;
            }
        } else {
            std::cout << "appReceive null\n";
        }
    });
    
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
    
    // 发送自定义数据包
    char packet[40096] = {};
    
    json jsonData = nlohmann::json::parse(jsonConfig);
    transport->send(jsonData,1,std::chrono::milliseconds(100));
    int len = transport->output(packet,sizeof(packet),std::chrono::milliseconds(100));

    send(sock, packet, len, 0);
    printf("TCP SEND:\r\n");
    print_packet((const uint8_t*)(packet),len);

//2.show tables
    jsonConfig = R"({
        "action": "show tables"
    })";
    
    jsonData = nlohmann::json::parse(jsonConfig);
    transport->send(jsonData,2,std::chrono::milliseconds(100));
    len = transport->output(packet,sizeof(packet),std::chrono::milliseconds(100));

    send(sock, packet, len, 0);
    printf("TCP SEND:\r\n");
    print_packet((const uint8_t*)(packet),len);
    
//3.insert rows
    jsonData["action"] = "insert";
    jsonData["name"] = "client-test";
    json jsonRows = json::array();
    for (int i=0;i<30;i++) {
        json row;
        row["id"] = i;
        row["name"] = "test name" + std::to_string(i);
        row["age"] = 20+i;
        row["addr"] = "street " + std::to_string(i);
        jsonRows.push_back(row);
    }
    jsonData["rows"] = jsonRows;
    //jsonData = nlohmann::json::parse(jsonConfig);
    transport->send(jsonData,3,std::chrono::milliseconds(100));
    len = transport->output(packet,sizeof(packet),std::chrono::milliseconds(100));

    send(sock, packet, len, 0);
    printf("TCP SEND:\r\n");
    print_packet((const uint8_t*)(packet),len);
//4.show rows
    jsonConfig = R"({
        "action": "get",
        "name": "client-test"
    })";
    
    jsonData = nlohmann::json::parse(jsonConfig);
    transport->send(jsonData,4,std::chrono::milliseconds(100));
    len = transport->output(packet,sizeof(packet),std::chrono::milliseconds(100));

    send(sock, packet, len, 0);
    printf("TCP SEND:\r\n");
    print_packet((const uint8_t*)(packet),len);

//n.tcp read timer
    Timer timer1(uv_default_loop(), 1000, 1000, [&]() {
        // 接收响应
        char buffer[1024] = {0};
        
        ssize_t bytes_read = read(sock, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            printf("TCP RECV:\r\n");
            print_packet((const uint8_t*)buffer,bytes_read);
            transport->input(buffer,bytes_read,std::chrono::milliseconds(1000));
            ;
        } else if (bytes_read == 0) {
            // 对端关闭连接
            std::cout << "Connection closed by peer." << std::endl;
            ;
        } else {
            // 读取发生错误，检查 errno
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::cout << "No data available now, try again later." << std::endl;
            } else if (errno == EINTR) {
                std::cout << "Interrupted by signal, retrying." << std::endl;
            } else {
                std::cerr << "Read error: " << strerror(errno) << std::endl;
            }
            ;
        }
    });

    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    timer.stop();
    // 关闭连接
    close(sock);
    auto wake_up_time = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    std::cout << "Sleeping until a specific time...\n";
    std::this_thread::sleep_until(wake_up_time);
    return 0;
}
