#include "transportsrv.hpp"

std::atomic<int> TransportSrv::ports_count(0);
std::atomic<int> TransportSrv::unique_id(0);
TransportSrv& transportSrv = TransportSrv::get_instance();

TransportSrv& TransportSrv::get_instance() {
    static TransportSrv instance;
    return instance;
}

TransportSrv::~TransportSrv() {
    // 遍历并清理所有连接
    {
        std::lock_guard<std::mutex> lock(mutex_);
        //std::cout << "TransportSrv instance destroyed!" << std::endl;
        ports_.clear(); // 清空 map
    }
}

std::shared_ptr<Transport> TransportSrv::get_transport(int port_id) {
	std::lock_guard<std::mutex> lock(mutex_);
    auto it = ports_.find(port_id);
    if (it != ports_.end()) {
        return it->second;
    }
    return nullptr; // Return nullptr if not found
}

std::vector<int> TransportSrv::get_all_ports() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<int> port_ids;
    port_ids.reserve(ports_.size()); // 预分配空间
    for (const auto& [port_id, _] : ports_) {
        port_ids.push_back(port_id);
    }
    return port_ids;
}

int TransportSrv::open_new_port(size_t buffer_size) {
	std::lock_guard<std::mutex> lock(mutex_);
	ports_count++;
	unique_id++;
	auto port = std::make_shared<Transport>(buffer_size);
	ports_.emplace(unique_id, port);
	return unique_id;
}

void TransportSrv::close_port(int port_id) {
	std::lock_guard<std::mutex> lock(mutex_);
	// 使用 unordered_map 的 find 方法快速查找
    auto it = ports_.find(port_id);
    if (it != ports_.end()) {
        //std::cout << "port[" << port_id << "] erased" << std::endl;
        ports_.erase(it);  // 从容器中移除
        --ports_count;     // 更新端口计数
    }
}