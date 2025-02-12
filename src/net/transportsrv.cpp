#include "transportsrv.hpp"


std::atomic<uint32_t> TransportSrv::ports_count(0);
uint32_t TransportSrv::unique_id(0);


TransportSrv::~TransportSrv() {
    // 遍历并清理所有连接
    {
        std::lock_guard<std::mutex> lock(mutex_);
        #ifdef DEBUG
        std::cout << "TransportSrv destroyed!" << std::endl;
        #endif
        ports_.clear(); // 清空 map
    }
    //work_guard_.reset();
}


void TransportSrv::on_new_connection(std::shared_ptr<TcpConnection> connection) {
    auto port_info = open_port();
    port_info.second->add_callback(connection);
    connection->set_transport_id(port_info.first);
    connection->set_transport(port_info.second);
}

void TransportSrv::start() {
    for (int i = 0; i < 2 ; ++i) {
		boost::asio::post(thread_pool_, [this,i]() {
			io_[i].run();
		});
	}
}

void TransportSrv::stop() {
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
    std::cout << "TransportSrv stopped." << std::endl;
#endif
}

std::vector<uint32_t> TransportSrv::get_all_ports() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<uint32_t> port_ids;
    port_ids.reserve(ports_.size()); // 预分配空间
    for (const auto& [port_id, _] : ports_) {
        port_ids.push_back(port_id);
    }
    return port_ids;
}

std::shared_ptr<Transport> TransportSrv::get_port(uint32_t port_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto port = ports_.find(port_id);
    if (port == ports_.end()) {
        std::cout << "Port " << std::to_string(port_id) << " not found\n";
        return nullptr;
    }
    return port->second;
}

trans_pair TransportSrv::open_port() {
	std::lock_guard<std::mutex> lock(mutex_);
	ports_count++;
	unique_id++;
    std::vector<boost::asio::io_context*> io_contexts = { &io_[0], &io_[1] };
    auto port = std::make_shared<Transport>(transport_buff_szie, io_contexts, unique_id);
	ports_.emplace(unique_id, port);
    this->notify_new_transport(port);
	return std::make_pair(unique_id,port);
}

void TransportSrv::close_port(uint32_t port_id) {
	std::lock_guard<std::mutex> lock(mutex_);
    
	// 使用 unordered_map 的 find 方法快速查找
    auto it = ports_.find(port_id);
    if (it != ports_.end()) {
        it->second->stop();
        #ifdef DEBUG
        std::cout << "transport[" << port_id << "] erased from list" << std::endl;
        #endif
        ports_.erase(it);  // 从容器中移除
        this->notify_close_transport(port_id);
        --ports_count;     // 更新端口计数
    }
}
