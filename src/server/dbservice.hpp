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
	timer(uv_default_loop(), keep_alv_timer, keep_alv_timer, [this]() {
        this->on_timer();
    }) {
		std::cout << "DBService start" << std::endl;
	}
	
	~DBService() {
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
		//std::cout << "on_new_transport" << transport->get_id() << std::endl;
		auto dbtask = std::make_shared<DbTask>(transport->get_id());
        transport->add_callback(dbtask);
    }

private:
	
    void on_msg(const std::shared_ptr<DBMsg> msg) override;
	void on_timer();

private:
	MemDatabase::ptr& db;
	Timer timer;
	static constexpr uint32_t keep_alv_timer = 60000;
};


#endif
