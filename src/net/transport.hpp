#ifndef TRANSPORT_HPP
#define TRANSPORT_HPP

#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <thread>
#include <nlohmann/json.hpp> // 使用 nlohmann/json 库解析 JSON
#include "util/msgbuffer.hpp"


using json = nlohmann::json;

// Msg 结构定义
struct MsgHeader {
    uint32_t length;     // 包含 payload 和 footer 的长度
    uint32_t msg_id;     // 消息 ID
    uint32_t segment_id; // 分段 ID
};

struct MsgFooter {
    uint32_t checksum;   // 校验和
};

struct Msg {
    MsgHeader header;
    std::vector<char> payload;
    MsgFooter footer;
};


class Transport {
public:
    enum class BufferType {
        APP2TCP,
        TCP2APP,
        ALLTYPE
    };
    Transport(size_t buffer_size)
        : app_to_tcp_(buffer_size), tcp_to_app_(buffer_size) {}

    // 1. APP 缓存到下行 CircularBuffer
    int appSend(const json& json_data, uint32_t msg_id, std::chrono::milliseconds timeout);
    // 2. TCP 读取下行 CircularBuffer
    int tcpReadFromApp(char* buffer, size_t size, std::chrono::milliseconds timeout);
    // 3. TCP 缓存到上行 CircularBuffer
    int tcpReceive(const char* buffer, size_t size, std::chrono::milliseconds timeout);
    // 4. APP 读取上行 CircularBuffer
    int appReceive(json& json_data, std::chrono::milliseconds timeout);

	void resetBuffers(BufferType type = BufferType::ALLTYPE) {
        if (BufferType::ALLTYPE == type) {
            app_to_tcp_.clear();
		    tcp_to_app_.clear();
        } else if (BufferType::APP2TCP == type) {
            app_to_tcp_.clear();
        } else if (BufferType::TCP2APP == type) {
            tcp_to_app_.clear();
        }
	}

private:
    static constexpr size_t segment_size_ = 1024; // 分段大小
    CircularBuffer app_to_tcp_; // 缓存上层发送的数据
    CircularBuffer tcp_to_app_; // 缓存下层接收的数据


	// 计算校验和
	uint32_t calculateChecksum(const std::vector<char>& data);

    // 序列化 Msg 为网络字节序
    std::vector<char> serializeMsg(const Msg& msg);

    // 反序列化网络字节序为 Msg
    Msg deserializeMsg(const std::vector<char>& buffer);
};
#endif