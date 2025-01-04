#ifndef TRANSPORTSRV_HPP
#define TRANSPORTSRV_HPP

#include <iostream>
#include <uv.h>
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
#include "transport.hpp"

class TransportSrv {
public:
    static TransportSrv& get_instance();
	std::shared_ptr<Transport> get_transport(int port_id);
	int open_new_port(size_t buffer_size);
	void close_port(int port_id);
private:
    TransportSrv() = default;
    ~TransportSrv();
	std::mutex mutex_;
	static std::atomic<int> unique_id;
	static std::atomic<int> ports_count;  // 连接数
	std::unordered_map<int, std::shared_ptr<Transport>> ports_;
};

extern TransportSrv& transportSrv; // 声明全局变量

#endif