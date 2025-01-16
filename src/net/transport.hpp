#ifndef TRANSPORT_HPP
#define TRANSPORT_HPP
#include <unistd.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <thread>
#include <variant>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <type_traits>
#include <nlohmann/json.hpp> // 使用 nlohmann/json 库解析 JSON
#include "util/msgbuffer.hpp"
#include "util/timer.hpp"


using json = nlohmann::json;
using transport_callback = std::function<void(char* buffer, int len, uint32_t port_id)>;
using transport_onread = std::function<void(std::vector<json>* json_datas, uint32_t port_id)>;

// Msg 结构定义
struct MsgHeader {
    uint32_t length;     // 包含 payload 和 footer 的长度
    uint32_t msg_id;     // 消息 ID
    uint32_t segment_id; // 分段 ID
    uint32_t flag;       // 分段 标志: 0 end, 1 not end
};

struct MsgFooter {
    uint32_t checksum;   // 校验和
};

struct Msg {
    MsgHeader header;
    std::vector<char> payload;
    MsgFooter footer;
};

struct MessageBuffer {
    std::map<uint32_t, std::vector<char>> segments; // 按 segment_id 存储分包
    uint32_t total_segments = 0;             // 总分包数
    size_t total_size = 0;                   // 当前消息的总大小
    bool is_complete = false;                // 是否已完成
    std::chrono::steady_clock::time_point last_update; // 最近更新的时间
};


// 定义数据类型的别名
// 定义可以在回调中处理的数据类型
using DataVariant = std::variant<
    std::tuple<char*, int, uint32_t>, 
    std::tuple<std::vector<json>*, uint32_t>
>;

// 定义接口类
class IDataCallback {
public:
    virtual void on_data_received(int result) = 0;  // 回调处理逻辑
    virtual DataVariant& get_data() = 0;  // 获取数据缓存
    virtual ~IDataCallback() = default; // 虚析构函数
};


class Transport {
public:
    enum class ChannelType {
        UP_LOW,
        LOW_UP,
        ALL
    };
    Transport(size_t buffer_size, boost::asio::io_context& io_context, uint32_t port_id = 0);
    ~Transport();
    void stop();
    uint32_t get_id() {
        return id_;
    }
    // 添加回调
    void add_callback(const std::shared_ptr<IDataCallback>& callback) {
        //std::cout << "add callback" << std::endl;
        callbacks_.push_back(callback);
    }

    // 1. APP 缓存到下行 CircularBuffer
    int send(const std::string& data, uint32_t msg_id, std::chrono::milliseconds timeout);
    // 2. TCP 读取下行 CircularBuffer
    int output(char* buffer, size_t size, std::chrono::milliseconds timeout);
    // 3. TCP 缓存到上行 CircularBuffer
    int input(const char* buffer, size_t size, std::chrono::milliseconds timeout);
    // 4. APP 读取上行 CircularBuffer
    int read(std::vector<json>& json_datas, std::chrono::milliseconds timeout);

	void reset(ChannelType type) {
        if (ChannelType::ALL == type) {
            app_to_tcp_.clear();
		    tcp_to_app_.clear();
        } else if (ChannelType::UP_LOW == type) {
            app_to_tcp_.clear();
        } else if (ChannelType::LOW_UP == type) {
            tcp_to_app_.clear();
        }
	}

private:
    static constexpr size_t segment_size_ = 1024; // 分段大小
    static constexpr size_t max_message_size_ = 10 * 1024 * 1024; // 限制最大消息大小为 10 MB
    static constexpr size_t max_cache_size = 16;  // 缓存的最大消息数量
    std::map<uint32_t, MessageBuffer> message_cache; // 缓存容器
    CircularBuffer app_to_tcp_; // 缓存上层发送的数据
    CircularBuffer tcp_to_app_; // 缓存下层接收的数据
    boost::asio::io_context& io_context_;
    Timer timer_[2];
    uint32_t id_;

    
    std::vector<std::shared_ptr<IDataCallback>> callbacks_; // 存储回调的容器

    void triger_event(ChannelType type);
    void on_send();
    void on_input();

	// 计算校验和
	uint32_t calculateChecksum(const std::vector<char>& data);

    // 序列化 Msg 为网络字节序
    std::vector<char> serializeMsg(const Msg& msg);

    // 反序列化网络字节序为 Msg
    Msg deserializeMsg(const std::vector<char>& buffer);
};
#endif