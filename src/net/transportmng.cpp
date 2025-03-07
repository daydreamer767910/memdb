#include "transportmng.hpp"
#include "util/version.hpp"

TransportMng::TransportMng() {

};

TransportMng::~TransportMng() {
    #ifdef DEBUG
    std::cout << "TransportMng destroyed!" << std::endl;
    #endif
}

void TransportMng::start() {
#ifdef DEBUG
    std::cout << "TransportMng " << PROJECT_VERSION << " start" << std::endl;
#endif
}

void TransportMng::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    ports_.clear(); // 清空 map
#ifdef DEBUG
    std::cout << "TransportMng stopped." << std::endl;
#endif
}

std::vector<uint32_t> TransportMng::get_all_ports() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<uint32_t> port_ids;
    port_ids.reserve(ports_.size()); // 预分配空间
    for (const auto& [port_id, _] : ports_) {
        port_ids.push_back(port_id);
    }
    return port_ids;
}

std::shared_ptr<Transport> TransportMng::get_port(uint32_t port_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto port = ports_.find(port_id);
    if (port == ports_.end()) {
        std::cout << "Port " << std::to_string(port_id) << " not found\n";
        return nullptr;
    }
    return port->second;
}

std::shared_ptr<Transport> TransportMng::open_port(uint32_t id) {
	std::lock_guard<std::mutex> lock(mutex_);
    auto port = std::make_shared<Transport>(id);
	ports_.emplace(id, port);
	return port;
}

void TransportMng::close_port(uint32_t id) {
	std::lock_guard<std::mutex> lock(mutex_);
    
	// 使用 unordered_map 的 find 方法快速查找
    auto it = ports_.find(id);
    if (it != ports_.end()) {
        it->second->stop();
        #ifdef DEBUG
        std::cout << "transport[" << id << "] erased from list" << std::endl;
        #endif
        ports_.erase(it);  // 从容器中移除
    }
}
