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
#include <uv.h>
#include <unordered_map>
#include <nlohmann/json.hpp> // 使用 nlohmann/json 库解析 JSON
#include "util/msgbuffer.hpp"


using json = nlohmann::json;
using transport_callback = std::function<void(char* buffer, int len)>;


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

class Transport {
public:
    enum class ChannelType {
        UP_LOW,
        LOW_UP,
        ALL
    };
    Transport(size_t buffer_size, uv_loop_t* loop);
    ~Transport();

    void set_callback(transport_callback cb,char* cb_buf, size_t cb_size) {
        output_callback_ = cb;
        buffer_callback_ = cb_buf;
        size_callback_ = cb_size;
    }

    // 1. APP 缓存到下行 CircularBuffer
    int send(const json& json_data, uint32_t msg_id, std::chrono::milliseconds timeout);
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
    uv_loop_t* loop_;
    uv_poll_t poll_handle;
    int pipe_fds[2];
    transport_callback output_callback_;
    char* buffer_callback_;
    size_t size_callback_;

    void triger_on_send();
    void on_send();
    static void process_event(uv_poll_t* handle, int status, int events);
	// 计算校验和
	uint32_t calculateChecksum(const std::vector<char>& data);

    // 序列化 Msg 为网络字节序
    std::vector<char> serializeMsg(const Msg& msg);

    // 反序列化网络字节序为 Msg
    Msg deserializeMsg(const std::vector<char>& buffer);
};
#endif