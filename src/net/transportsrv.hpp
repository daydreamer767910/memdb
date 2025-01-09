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

using trans_pair = std::pair<uint32_t,std::shared_ptr<Transport>>;
class TransportSrv {
public:
    static TransportSrv& get_instance();
	//获取所有打开的 port_id 列表
    std::vector<uint32_t> get_all_ports();

	trans_pair open_port(size_t buffer_size, uv_loop_t* loop);
	void close_port(uint32_t port_id);
	std::shared_ptr<Transport> get_port(uint32_t port_id);
private:
    TransportSrv() = default;
    ~TransportSrv();
	std::mutex mutex_;
	static uint32_t unique_id;
	static std::atomic<uint32_t> ports_count;  // 连接数
	std::unordered_map<uint32_t, std::shared_ptr<Transport>> ports_;

};
extern TransportSrv& transportSrv; // 声明全局变量

#endif