#include "transportmng.hpp"
#include "util/version.hpp"

size_t TransportMng::thread_pool_size_ = get_env_var("TRANSPORT_POOL_SIZE", size_t(4));
size_t TransportMng::transport_buff_size = get_env_var("TRANSPORT_BUFFER_SIZE", size_t(16*1024));

TransportMng::TransportMng() :thread_pool_(thread_pool_size_),
    work_guard_{boost::asio::make_work_guard(io_[0]), boost::asio::make_work_guard(io_[1]) } {
#ifdef DEBUG
    std::cout << "TransportMng " << PROJECT_VERSION << " start" << std::endl;
#endif
};

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

void TransportMng::start() {
    assert(thread_pool_size_>=2);
    for (int i = 0; i < thread_pool_size_ ; ++i) {
        if (i < thread_pool_size_ * 3 / 4) {  // 75% 线程给 RX
            boost::asio::post(thread_pool_, [this]() {
                io_[1].run();
            });
        } else {  // 25% 线程给 TX
            boost::asio::post(thread_pool_, [this]() {
                io_[0].run();
            });
        }
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
    auto port = std::make_shared<Transport>(transport_buff_size, io_contexts, id);
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
