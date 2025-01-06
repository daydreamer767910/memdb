#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "net/transportsrv.hpp"
#include "log/logger.hpp"
#include "util/util.hpp"

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
    
    // 发送自定义数据包
    char packet[1024] = {};
    auto transport = std::make_shared<Transport>(4096);
    json jsonData = nlohmann::json::parse(jsonConfig);
    transport->appSend(jsonData,1,std::chrono::milliseconds(100));
    int len = transport->tcpReadFromApp(packet,sizeof(packet),std::chrono::milliseconds(100));

    send(sock, packet, len, 0);
    printf("TCP SEND:");
    print_packet((const uint8_t*)(packet),len);
    
    while(true) {
        // 接收响应
        char buffer[1024] = {0};
        json recvJson;
        int valread = read(sock, buffer, 1024);
        printf("TCP RECV:");
        print_packet((const uint8_t*)buffer,valread);
        transport->tcpReceive(buffer,valread,std::chrono::milliseconds(100));
        if(-2 == transport->appReceive(recvJson,std::chrono::milliseconds(100)) ) {
            transport->resetBuffers(Transport::BufferType::TCP2APP);
            continue;
        }
        printf("APP RECV:");
        std::cout << recvJson.dump(4) << std::endl;

        auto wake_up_time = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        std::cout << "Sleeping until a specific time...\n";
        std::this_thread::sleep_until(wake_up_time);
    }
    // 关闭连接
    close(sock);

    return 0;
}
