#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <set>
#include <fstream>
#include <csignal>
#include <nlohmann/json.hpp>
#include <uv.h>
#include "dbcore/memtable.hpp"
#include "dbcore/memdatabase.hpp"
#include "net/tcpserver.hpp"
#include "log/logger.hpp"
#include "server/dbservice.hpp"



auto server = TcpServer("0.0.0.0", 7899);

void signal_handler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nSIGINT received. Preparing to exit..." << std::endl;
        logger.terminate();
        server.stop();
    }
}

int main() {
    // 设置信号处理程序
    std::signal(SIGINT, signal_handler);
    std::string logFile = std::string(std::getenv("HOME")) + "/log/mdbsrv.log";
    // 输出到文件
    logger.set_log_file(logFile);
    // 启用/禁用控制台输出
    logger.enable_console_output(true);
    logger.start();
    logger.log(Logger::LogLevel::INFO, "MDB service started ");
    DBService::getInstance()->start();
    transportSrv.add_observer(DBService::getInstance());
    // 启动 TCP 服务器
    server.start();

    std::cout << "Exiting program." << std::endl;
    return 0;
}
