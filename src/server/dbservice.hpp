#ifndef DBSERVICE_HPP
#define DBSERVICE_HPP

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <variant>
#include "util/threadbase.hpp"
#include "net/transport.hpp"
#include "util/timer.hpp"

using DBMsg = std::variant<std::tuple<int,int,json>>;
class DBService : public ThreadBase<std::tuple<int,int,json>>{
public:
	static DBService& get_instance();
private:
	
    void on_msg(const std::shared_ptr<DBMsg> msg) override;
	void process() override;


	void handle_task(int port_id,json jsonTask);

};
extern DBService& dbSrv; // 声明全局变量

#endif
