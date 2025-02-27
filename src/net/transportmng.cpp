#include "transportmng.hpp"

TransportMng::~TransportMng() {
    // 遍历并清理所有连接
    {
        std::lock_guard<std::mutex> lock(mutex_);
        #ifdef DEBUG
        std::cout << "TransportMng destroyed!" << std::endl;
        #endif
        ports_.clear(); // 清空 map
    }
    //work_guard_.reset();
}


void TransportMng::on_new_connection(const std::shared_ptr<IConnection>& connection) {
    auto port = open_port(connection->get_id());
    port->add_callback(connection);
    connection->set_transport(port);
    this->notify_new_transport(port);
}

void TransportMng::on_close_connection(const uint32_t port_id) {
    close_port(port_id);
    this->notify_close_transport(port_id);
}

void TransportMng::start() {
    for (int i = 0; i < 2 ; ++i) {
		boost::asio::post(thread_pool_, [this,i]() {
			io_[i].run();
		});
	}
}

void TransportMng::stop() {
    for (int i = 0; i < 2 ; ++i) {
        io_[i].stop();
    }
    
    thread_pool_.stop();
    for (int i = 0; i < 2 ; ++i) {
        work_guard_[i].reset();
    }
    // 等待线程池中的所有线程完成任务
    thread_pool_.join();
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
    std::vector<boost::asio::io_context*> io_contexts = { &io_[0], &io_[1] };
    auto port = std::make_shared<Transport>(transport_buff_szie, io_contexts, id);
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
