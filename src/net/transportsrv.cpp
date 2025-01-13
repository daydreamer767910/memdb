#include "transportsrv.hpp"


std::atomic<uint32_t> TransportSrv::ports_count(0);
uint32_t TransportSrv::unique_id(0);


TransportSrv::~TransportSrv() {
    // 遍历并清理所有连接
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "TransportSrv destroyed!" << std::endl;
        ports_.clear(); // 清空 map
    }
}


void TransportSrv::on_new_connection(std::shared_ptr<TcpConnection> connection) {
    auto port_info = open_port();
    port_info.second->add_callback(connection);
    connection->set_transport_id(port_info.first);
    connection->set_transport(port_info.second);
}

void TransportSrv::start() {
	std::promise<void> promise;
    std::future<void> future = promise.get_future();
	// Run the event loop in a new thread
    uv_eventLoopThread = std::thread([this](std::promise<void> promise) {
        uv_signal_t sig;
        // Set up signal listener
        uv_signal_init(loop_, &sig);
        uv_signal_start(&sig, [](uv_signal_t* handle, int signum) {
            std::cout << "Signal received: " << signum << std::endl;
            uv_stop(handle->loop); // Stop the loop
            uv_signal_stop(handle); // Stop the signal listener
            uv_close((uv_handle_t*)handle, nullptr); // Free resources
        }, SIGUSR1); // Capture SIGINT signal

        promise.set_value(); // Notify the main thread
        std::cout << "Event loop starting:" << std::this_thread::get_id() << std::endl;

        // Run the loop
        uv_run(loop_, UV_RUN_DEFAULT);

        // Clean up
        std::cout << "Event loop stopped." << std::endl;
        uv_loop_close(loop_);
    }, std::move(promise));
	future.wait(); // 等待子线程通知
}

void TransportSrv::stop() {
    pthread_t threadId = uv_eventLoopThread.native_handle();
	pthread_kill(threadId, SIGUSR1);
	if (uv_eventLoopThread.joinable())
		uv_eventLoopThread.join();
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

trans_pair TransportSrv::open_port(uv_loop_t* loop) {
	std::lock_guard<std::mutex> lock(mutex_);
	ports_count++;
	unique_id++;
	auto port = std::make_shared<Transport>(transport_buff_szie,loop?loop:loop_,unique_id);
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
        std::cout << "transport[" << port_id << "] erased from list" << std::endl;
        ports_.erase(it);  // 从容器中移除
        --ports_count;     // 更新端口计数
    }
}
