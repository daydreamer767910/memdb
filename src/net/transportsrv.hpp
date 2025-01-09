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
	int send(const json& json_data, uint32_t port_id, uint32_t timeout);
	int recv(json& json_data, uint32_t port_id, uint32_t timeout);
	int recv(std::vector<json>& json_datas, uint32_t port_id, uint32_t timeout);
	int input(const char* buffer, size_t size,uint32_t port_id, uint32_t timeout);
	int output(char* buffer, size_t size,uint32_t port_id, uint32_t timeout);
	void reset(uint32_t port_id, Transport::ChannelType type = Transport::ChannelType::ALL);
public:
    static TransportSrv& get_instance();
	//获取所有打开的 port_id 列表
    std::vector<uint32_t> get_all_ports();
	uint32_t open_port(size_t buffer_size, uv_loop_t* loop, transport_callback cb, char* cb_buf, size_t cb_size);
	void close_port(uint32_t port_id);
private:
    TransportSrv() = default;
    ~TransportSrv();
	//std::mutex mutex_;
	static std::atomic<uint32_t> unique_id;
	static std::atomic<uint32_t> ports_count;  // 连接数
	std::unordered_map<uint32_t, std::shared_ptr<Transport>> ports_;

};
extern TransportSrv& transportSrv; // 声明全局变量

#endif