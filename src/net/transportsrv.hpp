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

class ITransportObserver {
public:
    virtual void on_new_transport(const std::shared_ptr<Transport>& transport) = 0;
    virtual ~ITransportObserver() = default;
};


class TransportSrv {
public:
    static TransportSrv& get_instance();

	void add_observer(const std::shared_ptr<ITransportObserver>& observer) {
        observers_.push_back(observer);
    }

	//获取所有打开的 port_id 列表
    std::vector<uint32_t> get_all_ports();

	trans_pair open_port(size_t buffer_size, uv_loop_t* loop);
	void close_port(uint32_t port_id);
	std::shared_ptr<Transport> get_port(uint32_t port_id);
private:
    TransportSrv() {
		std::cout << "TransportSrv start" << std::endl;
	};
    ~TransportSrv();

	void notify_new_transport(const std::shared_ptr<Transport>& transport) {
        for (const auto& observer : observers_) {
            observer->on_new_transport(transport);
        }
    }

	std::mutex mutex_;
	static uint32_t unique_id;
	static std::atomic<uint32_t> ports_count;  // 连接数
	std::unordered_map<uint32_t, std::shared_ptr<Transport>> ports_;

	std::vector<std::shared_ptr<ITransportObserver>> observers_;

};
extern TransportSrv& transportSrv; // 声明全局变量

#endif