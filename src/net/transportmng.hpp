#ifndef TransportMng_HPP
#define TransportMng_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <tuple>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <thread>
#include <future>
#include <boost/asio.hpp>
#include "transport.hpp"
#include "tcpconnection.hpp"

class TransportMng: public IConnectionObserver {
public:
using ptr = std::shared_ptr<TransportMng>;

    static ptr& get_instance(){
		static ptr instance(new TransportMng());
		return instance;
	};
	TransportMng() :thread_pool_(2),
		work_guard_{boost::asio::make_work_guard(io_[0]), boost::asio::make_work_guard(io_[1]) } {
#ifdef DEBUG
		std::cout << "TransportMng start" << std::endl;
#endif
	};
    ~TransportMng();

	void add_observer(const std::shared_ptr<ITransportObserver>& observer) {
        observers_.push_back(observer);
    }

	//获取所有打开的 port_id 列表
    std::vector<uint32_t> get_all_ports();
	std::shared_ptr<Transport> get_port(uint32_t port_id);

	void start();
	void stop();
	void on_new_connection(const std::shared_ptr<IConnection>& connection) override;
	void on_close_connection(const uint32_t port_id) override;
private:
	void notify_new_transport(const std::shared_ptr<Transport>& transport) {
        for (const auto& observer : observers_) {
            observer->on_new_transport(transport);
        }
    }
	void notify_close_transport(const uint32_t port_id) {
        for (const auto& observer : observers_) {
            observer->on_close_transport(port_id);
        }
    }

	std::shared_ptr<Transport> open_port();
	void close_port(uint32_t port_id);

	static constexpr size_t transport_buff_szie = 1024*9; //9K
	std::mutex mutex_;
	static uint32_t unique_id;
	static std::atomic<uint32_t> ports_count;  // 连接数
	std::unordered_map<uint32_t, std::shared_ptr<Transport>> ports_;
	boost::asio::io_context io_[2];
	boost::asio::thread_pool thread_pool_;
	std::vector<std::shared_ptr<ITransportObserver>> observers_;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_[2];
};

#endif