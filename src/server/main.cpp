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
#include "net/transportsrv.hpp"
#include "log/logger.hpp"
#include "server/dbservice.hpp"

bool exiting = false;
DBService::ptr db_server = DBService::getInstance();
auto transportSrv = TransportSrv::get_instance();


void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nsignal " << signal << " received. Preparing to exit..." << std::endl;
        logger.terminate();
        db_server->terminate();
        transportSrv->stop();
        exiting = true;
    }
}

int main(int argc, char* argv[]) {
    std::string srvIP = get_env_var("COMM_ADDR", std::string("0.0.0.0"));
    int srvPort = get_env_var("COMM_PORT", int(7900));
    // 设置信号处理程序
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::string logFile = get_env_var("LOG_FILE_PATH", std::string(std::string(std::getenv("HOME")) + "/log/mdbsrv.log"));
    // 输出到文件
    logger.set_log_file(logFile);
    // 启用/禁用控制台输出
    logger.enable_console_output(true);
    logger.start();
    
    db_server->start();
    transportSrv->add_observer(db_server);
    
    // 启动 TCP 服务器
    if (argc == 3) {
        // 如果有两个额外的参数传入
        srvIP = argv[1];
        srvPort = std::stoi(argv[2]);
    }
    transportSrv->start(srvIP, srvPort);
    while (!exiting) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    std::cout << "Exiting program." << std::endl;
    return 0;
}
