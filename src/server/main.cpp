#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <set>
#include <fstream>
#include <csignal>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include "dbcore/table.hpp"
#include "dbcore/database.hpp"
#include "net/tcpserver.hpp"
#include "log/logger.hpp"
#include "server/dbservice.hpp"


DBService::ptr db_server = DBService::getInstance();
TransportSrv::ptr tranport_server = TransportSrv::get_instance();
auto server = TcpServer();

void signal_handler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nSIGINT received. Preparing to exit..." << std::endl;
        logger.terminate();
        db_server->terminate();
        tranport_server->stop();
        server.stop();
    }
}

int main() {
    // 设置信号处理程序
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::string logFile = std::string(std::getenv("HOME")) + "/log/mdbsrv.log";
    // 输出到文件
    logger.set_log_file(logFile);
    // 启用/禁用控制台输出
    logger.enable_console_output(true);
    logger.start();
    
    db_server->start();
    tranport_server->add_observer(db_server);
    tranport_server->start();
    // 启动 TCP 服务器
    const char* port_id = std::getenv("COMM_PORT");
    const char* ip = std::getenv("COMM_ADDR");
    server.start(ip?ip:"0.0.0.0", port_id?std::stoi(port_id):7899);

    std::cout << "Exiting program." << std::endl;
    return 0;
}
