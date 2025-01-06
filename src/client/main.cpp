#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "net/transportsrv.hpp"
#include "log/logger.hpp"

Logger& logger = Logger::get_instance(); // 定义全局变量
TransportSrv& transportSrv = TransportSrv::get_instance(); // 定义全局变量
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

    // 发送自定义数据包
    unsigned char packet[] = {0x01, 0x02, 0x03, 0x04};
    send(sock, packet, sizeof(packet), 0);

    // 接收响应
    char buffer[1024] = {0};
    int valread = read(sock, buffer, 1024);
    std::cout << "Received: " << buffer << std::endl;

    // 关闭连接
    close(sock);

    return 0;
}
