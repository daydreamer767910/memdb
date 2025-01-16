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

using DBMsg = std::tuple<std::shared_ptr<std::vector<json>>,uint32_t>;
using DBVariantMsg = std::variant<DBMsg>;
class DBService : public ThreadBase<DBMsg>, public ITransportObserver{
public:
	DBService();
	
	~DBService();
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
		std::lock_guard<std::mutex> lock(mutex_);
		//std::cout << "on_new_transport" << transport->get_id() << std::endl;
		auto dbtask = std::make_shared<DbTask>(transport->get_id(),io_);
		tasks_.emplace(transport->get_id(), dbtask);
        transport->add_callback(dbtask);
    }

	void on_close_transport(const uint32_t port_id) override {
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = tasks_.find(port_id);
		if (it != tasks_.end()) {
			std::cout << "task[" << port_id << "] erased from list" << std::endl;
			tasks_.erase(it);  // 从容器中移除
		}
    }

	void save_db();
	void load_db();

private:
	void keep_alive();
    void on_msg(const std::shared_ptr<DBVariantMsg> msg) override;
	void on_timer(int tick, int time, std::thread::id id);

private:
	boost::asio::thread_pool thread_pool_;
	boost::asio::io_context io_;
	MemDatabase::ptr& db;
	Timer timer;
	//std::thread timer_thread_;    // 独立线程用于处理事件循环
	std::mutex mutex_;
	std::unordered_map<uint32_t, std::shared_ptr<DbTask>> tasks_;
	static constexpr uint32_t keep_alv_timer = 30000;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
};


#endif
