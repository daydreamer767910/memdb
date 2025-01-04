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
    Transport(size_t buffer_size)
        : app_to_tcp_(buffer_size), tcp_to_app_(buffer_size) {}

    // 1. APP 缓存到下行 CircularBuffer
    bool appSend(const json& json_data, uint32_t msg_id, std::chrono::milliseconds timeout) {
        std::string serialized = json_data.dump();
        size_t total_size = serialized.size();
        size_t segment_id = 0;

        size_t offset = 0;
        while (offset < total_size) {
            size_t chunk_size = std::min(total_size - offset, segment_size_);

            Msg msg;
            msg.header.length = chunk_size + sizeof(MsgFooter);
            msg.header.msg_id = msg_id;
            msg.header.segment_id = segment_id++;

            msg.payload.assign(serialized.begin() + offset, serialized.begin() + offset + chunk_size);
            msg.footer.checksum = calculateChecksum(msg.payload);

            std::vector<char> network_data = serializeMsg(msg);
            if (!app_to_tcp_.write(network_data.data(), network_data.size(), timeout)) {
                return false; // 写入超时
            }
            offset += chunk_size;
        }
        return true;
    }

    // 2. TCP 读取下行 CircularBuffer
    bool tcpReadFromApp(char* buffer, size_t size, std::chrono::milliseconds timeout) {
        return app_to_tcp_.read(buffer, size, timeout);
    }

    // 3. TCP 缓存到上行 CircularBuffer
    bool tcpReceive(const char* buffer, size_t size, std::chrono::milliseconds timeout) {
        return tcp_to_app_.write(buffer, size, timeout);
    }

    // 4. APP 读取上行 CircularBuffer
    bool appReceive(json& json_data, std::chrono::milliseconds timeout) {
        std::vector<char> temp_buffer(segment_size_);
        std::string reconstructed_data;

        while (true) {
            if (!tcp_to_app_.read(temp_buffer.data(), temp_buffer.size(), timeout)) {
                return false; // 读取超时
            }

            Msg msg = deserializeMsg(temp_buffer);
            reconstructed_data.append(msg.payload.begin(), msg.payload.end());

            if (msg.header.segment_id == 0) { // 最后一段
                break;
            }
        }

        json_data = json::parse(reconstructed_data);
        return true;
    }

	void resetBuffers() {
		app_to_tcp_.clear();
		tcp_to_app_.clear();
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