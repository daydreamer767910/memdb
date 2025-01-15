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
#include "dbcore/memdatabase.hpp"
#include "util/threadbase.hpp"
#include "net/transport.hpp"
#include "net/transportsrv.hpp"
#include "util/timer.hpp"
#include "dbtask.hpp"

using DBMsg = std::variant<std::tuple<int,int,json>>;
class DBService : public ThreadBase<std::tuple<int,int,json>>, public ITransportObserver{
public:
	DBService() :db(MemDatabase::getInstance()),
	timer(io_, keep_alv_timer, true, [this](int tick, int time, std::thread::id id) {
        this->on_timer(tick,time,id);
    }) {
		std::cout << "DBService start" << std::endl;
		std::string baseDir = std::string(std::getenv("HOME"));
		std::filesystem::path fullPath = std::filesystem::path(baseDir) / std::string("data");
		db->upload(fullPath.string());
		// 启动定时器
        timer.start();

        // 启动事件循环
        timer_thread_ = std::thread([this]() {
			std::cout << "DBService loop starting:" << std::this_thread::get_id() << std::endl;
            io_.run();
        });
	}
	
	~DBService() {
		if (timer_thread_.joinable()) {
            io_.stop();  // 停止事件循环
            timer_thread_.join();  // 等待线程退出
        }
		#ifdef DEBUG
		std::cout << "DB service destoryed!" << std::endl;
		#endif
	}
	DBService(const DBService&) = delete;
    DBService& operator=(const DBService&) = delete;

	using ptr = std::shared_ptr<DBService>;

    // 获取单例实例
    static ptr& getInstance() {
        static ptr instance(new DBService());
        return instance;
    }
	
	static MemDatabase* getDb() {
		return DBService::getInstance()->db.get();
	}

	void on_new_transport(const std::shared_ptr<Transport>& transport) override {
		std::cout << "on_new_transport" << transport->get_id() << std::endl;
		auto dbtask = std::make_shared<DbTask>(transport->get_id());
        transport->add_callback(dbtask);
    }

private:
	void keep_alive();
    void on_msg(const std::shared_ptr<DBMsg> msg) override;
	void on_timer(int tick, int time, std::thread::id id);

private:
	boost::asio::io_context io_;
	MemDatabase::ptr& db;
	Timer timer;
	std::thread timer_thread_;    // 独立线程用于处理事件循环
	static constexpr uint32_t keep_alv_timer = 30000;
};


#endif
