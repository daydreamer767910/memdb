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

class TransportMng {
public:
using ptr = std::shared_ptr<TransportMng>;

	static ptr& get_instance() {
		thread_pool_size_ = get_env_var("TRANSPORT_POOL_SIZE", size_t(4));
		transport_buff_size = get_env_var("TRANSPORT_BUFFER_SIZE", size_t(16*1024));
		static ptr instance = std::make_shared<TransportMng>();
		return instance;
	}
	TransportMng();
    ~TransportMng();

	//获取所有打开的 port_id 列表
    std::vector<uint32_t> get_all_ports();
	std::shared_ptr<Transport> get_port(uint32_t port_id);

	void start();
	void stop();
	std::shared_ptr<Transport> open_port(uint32_t id);
	void close_port(uint32_t id);
private:
	static size_t transport_buff_size; //16K
	static size_t thread_pool_size_;
	std::mutex mutex_;
	std::unordered_map<uint32_t, std::shared_ptr<Transport>> ports_;
	boost::asio::io_context io_[2];
	boost::asio::thread_pool thread_pool_;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_[2];
};

#endif