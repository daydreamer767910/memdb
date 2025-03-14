#ifndef TRANSPORT_HPP
#define TRANSPORT_HPP
#include <unistd.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>
#include <mutex>
#include <array>
#include <thread>
#include <boost/asio.hpp>
#include "util/msgbuffer.hpp"
#include "util/timer.hpp"
#include "common.hpp"
#include "crypt.hpp"

constexpr uint32_t FLAG_SEGMENTED   = 0b0001; // 1 << 0 , 0: end, 1: not end
constexpr uint32_t FLAG_ENCRYPTED   = 0b0010; // 1 << 1 , 0: 明文, 1: 加密
constexpr uint32_t FLAG_KEY_UPDATE  = 0b0100; // 1 << 2 , 0: 不切换, 1: 需要切换Key
constexpr uint32_t FLAG_COMPRESSED  = 0b1000; // 1 << 3 , 0: 不压缩, 1: 压缩
// Msg 结构定义
struct MsgHeader {
    uint32_t length;     // 包含 payload 和 footer 的长度
    uint32_t msg_id;     // 消息 ID
    uint32_t segment_id; // 分段 ID
    uint32_t flag;       // 分段 
};

struct MsgFooter {
    uint32_t checksum;   // 校验和
};

struct Msg {
    MsgHeader header;
    std::vector<uint8_t> payload;
    MsgFooter footer;
};

struct MessageBuffer {
    std::map<uint32_t, std::vector<uint8_t>> segments; // 按 segment_id 存储分包
    uint32_t total_segments = 0;             // 总分包数
    size_t total_size = 0;                   // 当前消息的总大小
    bool is_complete = false;                // 是否已完成
    bool is_compressed = false;              // 是否压缩
    std::chrono::steady_clock::time_point last_update = std::chrono::steady_clock::time_point::min();; // 最近更新的时间
};


class Transport {
public:
    enum class ChannelType {
        UP_LOW,
        LOW_UP,
        ALL
    };
    Transport(/*boost::asio::io_context& io_ctx_rx, boost::asio::io_context& io_ctx_tx, */uint32_t id);
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

    size_t getMessageSize() const {
        return max_message_size_;
    }

    void setEncryptMode(const bool& mode) {
        encryptMode_ = mode;
    }

    void setCompressFlag(const bool& flag) {
        compressFlag_ = flag;
    }

    void setSessionKeys(const std::vector<uint8_t>& rxKey, const std::vector<uint8_t>& txKey, const bool updateImmediately = false) {
        sessionKey_rx_new_ = rxKey;
        sessionKey_tx_new_ = txKey;
        if (updateImmediately) {
            updateKey_ = true;
            switchToNewKeys();
        }
    }

    void getSessionKeys(std::vector<uint8_t>& rxKey, std::vector<uint8_t>& txKey) {
        rxKey = sessionKey_rx_;
        txKey = sessionKey_tx_;
    }

    // 1. APP 缓存到下行 CircularBuffer
    int send(const uint8_t* data, size_t size, uint32_t msg_id, std::chrono::milliseconds timeout);
    // 2. TCP 读取下行 CircularBuffer
    int output(char* buffer, size_t size, std::chrono::milliseconds timeout);
    // 3. TCP 缓存到上行 CircularBuffer
    int input(const char* buffer, size_t size, std::chrono::milliseconds timeout);
    // 4. APP 读取上行 CircularBuffer
    int read(uint8_t* data, uint32_t& msg_id, size_t size, std::chrono::milliseconds timeout);

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
    static constexpr size_t max_cache_size_ = 8;
    static constexpr size_t segment_size_ = TCP_BUFFER_SIZE;// 分段大小
    static constexpr size_t encrypt_size_increment_ = AES_GCM_nonce_len + AES_GCM_tag_len;
    static size_t max_message_size_;// = 10 * 1024*1024; // 限制最大消息大小为 10 MB
    static constexpr size_t circular_buffer_size_ = 32*1024; //32k
    static uint32_t message_timeout_; // = 200; // 200ms
    
    std::mutex mutex_[2];
    std::map<uint32_t, MessageBuffer> message_cache; // 缓存容器
    CircularBuffer app_to_tcp_; // 缓存上层发送的数据
    CircularBuffer tcp_to_app_; // 缓存下层接收的数据
    //boost::asio::io_context* io_context_[2];
    //Timer timer_[2];
    uint32_t id_;
    bool encryptMode_;
    bool updateKey_;
    bool compressFlag_;
    //std::vector<unsigned char> local_secretKey_{std::vector<unsigned char>(crypto_box_SECRETKEYBYTES)};
    //std::vector<unsigned char> remote_publicKey_{std::vector<unsigned char>(crypto_box_PUBLICKEYBYTES)};
    std::vector<unsigned char> sessionKey_rx_, sessionKey_rx_new_;
    std::vector<unsigned char> sessionKey_tx_, sessionKey_tx_new_;
    
    std::vector<std::weak_ptr<IDataCallback>> callbacks_; // 存储回调的容器

    void triger_event(ChannelType type);
    void on_send();
    void on_input();

	// 计算校验和
	uint32_t calculateChecksum(const std::vector<uint8_t>& data);

    // 序列化 Msg 为网络字节序
    std::vector<char> serializeMsg(const Msg& msg);

    // 反序列化网络字节序为 Msg
    Msg deserializeMsg(const std::vector<char>& buffer);

    void switchToNewKeys() {
        sessionKey_rx_ = sessionKey_rx_new_;
        sessionKey_tx_ = sessionKey_tx_new_;
    }
};
#endif