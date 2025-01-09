#include "transportsrv.hpp"

std::atomic<uint32_t> TransportSrv::ports_count(0);
std::atomic<uint32_t> TransportSrv::unique_id(0);
TransportSrv& transportSrv = TransportSrv::get_instance();

TransportSrv& TransportSrv::get_instance() {
    static TransportSrv instance;
    return instance;
}

TransportSrv::~TransportSrv() {
    // 遍历并清理所有连接
    {
        //std::lock_guard<std::mutex> lock(mutex_);
        //std::cout << "TransportSrv instance destroyed!" << std::endl;
        ports_.clear(); // 清空 map
    }
}


std::vector<uint32_t> TransportSrv::get_all_ports() {
    //std::lock_guard<std::mutex> lock(mutex_);
    std::vector<uint32_t> port_ids;
    port_ids.reserve(ports_.size()); // 预分配空间
    for (const auto& [port_id, _] : ports_) {
        port_ids.push_back(port_id);
    }
    return port_ids;
}

uint32_t TransportSrv::open_port(size_t buffer_size, uv_loop_t* loop,
    transport_callback cb, char* cb_buf, size_t cb_size) {
	//std::lock_guard<std::mutex> lock(mutex_);
	ports_count++;
	unique_id++;
	auto port = std::make_shared<Transport>(buffer_size,loop);
    port->set_callback(cb,cb_buf,cb_size);
	ports_.emplace(unique_id, port);
	return unique_id;
}

void TransportSrv::close_port(uint32_t port_id) {
	//std::lock_guard<std::mutex> lock(mutex_);
	// 使用 unordered_map 的 find 方法快速查找
    auto it = ports_.find(port_id);
    if (it != ports_.end()) {
        //std::cout << "port[" << port_id << "] erased" << std::endl;
        ports_.erase(it);  // 从容器中移除
        --ports_count;     // 更新端口计数
    }
}

int TransportSrv::send(const json& json_data, uint32_t port_id, uint32_t timeout) {
    //std::lock_guard<std::mutex> lock(mutex_);
    int ret = 0;
    // Broadcast mode or specific port
    bool is_broadcast = (port_id == 0xffffffff);
    int count = 0;
    for (const auto& [id, port] : ports_) {
        if (is_broadcast || id == port_id) {
            ret = port->send(json_data, 1, std::chrono::milliseconds(timeout));
            // In non-broadcast mode, break after sending to the specific port
            if (!is_broadcast) {
                return ret;
            } else if (ret > 0) {
                count++;
            }
        }
    }

    return count;
}

int TransportSrv::recv(json& json_data, uint32_t port_id, uint32_t timeout) {
    //std::lock_guard<std::mutex> lock(mutex_);
    int ret = 0;
    
    for (const auto& [id, port] : ports_) {
        if ( id == port_id) {
            ret = port->read(json_data, std::chrono::milliseconds(timeout));
            break;
        }
    }
    return ret;
}

int TransportSrv::recv(std::vector<json>& json_datas, uint32_t port_id, uint32_t timeout) {
    //std::lock_guard<std::mutex> lock(mutex_);
    int ret = 0;
    
    for (const auto& [id, port] : ports_) {
        if ( id == port_id) {
            ret = port->read_all(json_datas, std::chrono::milliseconds(timeout));
            break;
        }
    }
    return ret;
}

int TransportSrv::input(const char* buffer, size_t size,uint32_t port_id, uint32_t timeout) {
    //std::lock_guard<std::mutex> lock(mutex_);
    int ret = 0;
    
    for (const auto& [id, port] : ports_) {
        if ( id == port_id) {
            ret = port->input(buffer,size, std::chrono::milliseconds(timeout));
            break;
        }
    }
    return ret;
}

int TransportSrv::output(char* buffer, size_t size,uint32_t port_id, uint32_t timeout) {
    //std::lock_guard<std::mutex> lock(mutex_);
    int ret = 0;
    
    for (const auto& [id, port] : ports_) {
        if ( id == port_id) {
            ret = port->output(buffer,size, std::chrono::milliseconds(timeout));
            break;
        }
    }
    return ret;
}

void TransportSrv::reset(uint32_t port_id, Transport::ChannelType type) {
    //std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [id, port] : ports_) {
        if ( id == port_id) {
            port->reset(type);
            break;
        }
    }
}