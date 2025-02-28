#include <cstring>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <netinet/in.h> // htons, htonl, ntohs, ntohl
#include <zlib.h>
#include "transport.hpp"
#include "util/util.hpp"

Transport::Transport(size_t buffer_size, const std::vector<boost::asio::io_context*>& io_contexts, uint32_t id)
    : app_to_tcp_(buffer_size),
      tcp_to_app_(buffer_size),
      io_context_{io_contexts[0], io_contexts[1]},  // 初始化指针数组
      timer_{ Timer(*io_context_[0], 0, false, [this](int, int, std::thread::id) { this->on_send(); }),
              Timer(*io_context_[1], 0, false, [this](int, int, std::thread::id) { this->on_input(); }) },
      id_(id),
      encryptMode_(false),
      compressFlag_(false) {
    sessionKey_rx_.resize(SESSION_KEY_SIZE);
    sessionKey_tx_.resize(SESSION_KEY_SIZE);
    sessionKey_rx_new_.resize(SESSION_KEY_SIZE);
    sessionKey_tx_new_.resize(SESSION_KEY_SIZE);
}


Transport::~Transport() {
    #ifdef DEBUG
	std::cout << "transport " << this->id_ << " destoryed" << std::endl;
    #endif
}

void Transport::stop() {
    callbacks_.clear();
    #ifdef DEBUG
    std::cout << "transport " << this->id_ << " stop" << std::endl;
    #endif
}


void Transport::on_send() {
	for (auto& callback_weak : callbacks_) {
        if (auto callback = callback_weak.lock()) {  // 直接解锁，不创建额外的 shared_ptr
            try {
                DataVariant& variant = callback->get_data();
                // 仅在数据为 tcpMsg 时才处理
                if (std::holds_alternative<tcpMsg>(variant)) {
                    auto& [buffer, buffer_size, port_id] = std::get<tcpMsg>(variant);
                    
                    if (port_id == this->id_) {
                        do {
                            int len = this->output(buffer, buffer_size, std::chrono::milliseconds(50));
                            if (len > 0) {
                                callback->on_data_received(len, 0);
                            } else {
                                break;
                            }
                        } while (true);
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Callback processing error: " << e.what() << std::endl;
            }
        }
    }    
}

void Transport::on_input() {
    for (auto& callback_weak : callbacks_) {
        auto callback = callback_weak.lock();
        if (!callback) continue;
        try {
            // 获取回调绑定的数据
            DataVariant& variant = callback->get_data();
            if (std::holds_alternative<appMsg>(variant)) {
                auto& data = std::get<appMsg>(variant);
                auto [app_data, max_cache_size, port_id] = data;
    
                if (port_id == 0xffffffff || port_id == this->id_) {
                    while (true) {
                        uint32_t id;
                        int len = this->read(app_data->data(), id, max_cache_size, std::chrono::milliseconds(50));
                        if (len > 0) {
                            callback->on_data_received(len, id);
                        } else {
                            break;
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Callback processing error: " << e.what() << std::endl;
        }
    }    
}

void Transport::triger_event(ChannelType type) {
    
    if (ChannelType::ALL == type) {
        timer_[0].start();
        timer_[1].start();
    } else if (ChannelType::UP_LOW == type) {
        /*boost::asio::post(*io_context_[0], [this]() {
            this->on_send();
        });*/
        timer_[0].start();
    } else if (ChannelType::LOW_UP == type) {
        timer_[1].start();
        /*boost::asio::post(*io_context_[1], [this]() {
            this->on_input();
        });*/
    }
}

// 计算校验和
uint32_t Transport::calculateChecksum(const std::vector<uint8_t>& data) {
    static const uint32_t polynomial = 0xEDB88320;
    static uint32_t crc_table[256];
    static bool table_generated = false;

    if (!table_generated) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t crc = i;
            for (uint32_t j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (polynomial & (~(crc & 1) + 1));
            }
            crc_table[i] = crc;
        }
        table_generated = true;
    }

    uint32_t crc = 0xFFFFFFFF;
    for (uint8_t c : data) {
        crc = (crc >> 8) ^ crc_table[(crc & 0xFF) ^ c];
    }

    return ~crc;
}

// 序列化 Msg 为网络字节序
std::vector<char> Transport::serializeMsg(const Msg& msg) {
    std::vector<char> buffer(sizeof(MsgHeader) + msg.payload.size() + sizeof(MsgFooter));
        size_t offset = 0;

        // 序列化消息头
        MsgHeader header_net = {
            htonl(msg.header.length),
            htonl(msg.header.msg_id),
            htonl(msg.header.segment_id),
			htonl(msg.header.flag),
        };
        std::memcpy(buffer.data() + offset, &header_net, sizeof(MsgHeader));
        offset += sizeof(MsgHeader);

        // 序列化负载
        if (!msg.payload.empty()) {
            std::memcpy(buffer.data() + offset, msg.payload.data(), msg.payload.size());
            offset += msg.payload.size();
        }

        // 序列化消息尾
        MsgFooter footer_net = {htonl(msg.footer.checksum)};
        std::memcpy(buffer.data() + offset, &footer_net, sizeof(MsgFooter));

        return buffer;
}

// 反序列化网络字节序为 Msg
Msg Transport::deserializeMsg(const std::vector<char>& buffer) {
    if (buffer.size() < sizeof(MsgHeader) + sizeof(MsgFooter)) {
        throw std::runtime_error("Invalid Msg size");
    }

    Msg msg;
    size_t offset = 0;

    // 反序列化消息头并转换为主机字节序
    MsgHeader header_net;
    std::memcpy(&header_net, buffer.data() + offset, sizeof(MsgHeader));
    offset += sizeof(MsgHeader);

    msg.header.length = ntohl(header_net.length); // 转换为主机字节序
    msg.header.msg_id = ntohl(header_net.msg_id);
    msg.header.segment_id = ntohl(header_net.segment_id);
	msg.header.flag = ntohl(header_net.flag);

    // 反序列化负载
    size_t payload_size = buffer.size() - sizeof(MsgHeader) - sizeof(MsgFooter);
    if (payload_size > 0) {
        msg.payload.resize(payload_size);
        std::memcpy(msg.payload.data(), buffer.data() + offset, payload_size);
        offset += payload_size;
    }

    // 反序列化消息尾并转换为主机字节序
    MsgFooter footer_net;
    std::memcpy(&footer_net, buffer.data() + offset, sizeof(MsgFooter));
    msg.footer.checksum = ntohl(footer_net.checksum);
    return msg;
}


int Transport::send(const uint8_t* data, size_t size, uint32_t msg_id, std::chrono::milliseconds timeout) {
    std::vector<uint8_t> compressedData;
    const uint8_t* dataToSend = data;
    size_t total_size = size;
    size_t segment_id = 0;
    size_t offset = 0;

    if (total_size > max_message_size_) {
        std::cerr << "Message size : " << total_size << " too big than buffer size: " << max_message_size_ << std::endl;
        return -3;
    }

    if (compressFlag_) {
        //std::vector<unsigned char> dataVector(data, data + size);
        int compressResult = compressData(data, size, compressedData);
        if (compressResult != Z_OK) {
            std::cerr << "Compression failed! Error code: " << compressResult << std::endl;
            return -4;
        }
        dataToSend = compressedData.data();
        total_size = compressedData.size();
    }

	while (offset < total_size) {
        size_t chunk_size;
        Msg msg;
        std::memset(&msg, 0, sizeof(Msg));
        msg.header.msg_id = msg_id;
		msg.header.segment_id = segment_id++;
        if (updateKey_) {
            //密钥已经更新
            msg.header.flag |= FLAG_KEY_UPDATE;
            updateKey_ = false;
        }
        if (compressFlag_) {
            msg.header.flag |= FLAG_COMPRESSED;
        }
        if (encryptMode_) {
            msg.header.flag |= FLAG_ENCRYPTED;
            chunk_size = std::min(total_size - offset, 
                segment_size_-sizeof(MsgHeader)-sizeof(MsgFooter)-encrypt_size_increment_);
            msg.payload.assign(dataToSend + offset, dataToSend + offset + chunk_size);
            std::vector<unsigned char> encrypted_segment;
            encryptData(sessionKey_tx_, msg.payload, encrypted_segment);
            // 计算加密后的 segment 长度（包含加密后的数据和 MAC 校验码等）
            size_t encrypted_size = encrypted_segment.size();
            msg.header.length = encrypted_size + sizeof(MsgHeader) + sizeof(MsgFooter);
            // 将加密后的数据赋值给 msg.payload
            msg.payload.clear();  // 避免意外情况
            msg.payload = std::move(encrypted_segment);
        } else {
            chunk_size = std::min(total_size - offset, segment_size_-sizeof(MsgHeader) - sizeof(MsgFooter));
            msg.header.length = chunk_size + sizeof(MsgHeader) + sizeof(MsgFooter);
            msg.payload.assign(dataToSend + offset, dataToSend + offset + chunk_size);
        }
        // **修正 FLAG_SEGMENTED**
        if (offset + chunk_size >= total_size) {
            msg.header.flag &= ~FLAG_SEGMENTED;  // 最后一片
        } else {
            msg.header.flag |= FLAG_SEGMENTED;
        }

		msg.footer.checksum = calculateChecksum(msg.payload);
        // Print CRC in hexadecimal format
        /*std::cout << "CRC32: 0x" << std::hex 
            << std::uppercase << std::setfill('0') 
            << std::setw(8) << msg.footer.checksum << std::endl;*/


		std::vector<char> network_data = serializeMsg(msg);
        int ret = app_to_tcp_.write(network_data.data(), network_data.size(), timeout);
        triger_event(ChannelType::UP_LOW);
		if (ret<0) {
            //retry 1 time
            #ifdef DEBUG
            std::cout << "app -> CircularBuffer fail and retry" << std::endl;
            #endif
            ret = app_to_tcp_.write(network_data.data(), network_data.size(), timeout);
            if(ret<0) {
                std::cerr << "app -> CircularBuffer fail" << std::endl;
                return -1; // 写入超时
            }
            triger_event(ChannelType::UP_LOW);
		}
		//std::cout << "APP->PORT :" << std::this_thread::get_id() << std::endl;
		//print_packet(reinterpret_cast<const uint8_t*>(network_data.data()), network_data.size());
		offset += chunk_size;
	}
	return total_size;
}

int Transport::output(char* buffer, size_t size, std::chrono::milliseconds timeout) {
	//读header的length字段
	size_t dataLen = app_to_tcp_.readableSize();
	//std::cout << "tcpReadFromApp:readableSize:" << dataLen << std::endl;
	if (dataLen > 0) {
		size = std::min(dataLen,size);
	}
	return app_to_tcp_.read(buffer, size, timeout);
}


int Transport::input(const char* buffer, size_t size, std::chrono::milliseconds timeout) {
	//std::cout << "Transport::input: \n";
	//print_packet(reinterpret_cast<const uint8_t*>(buffer),size);
	int ret = tcp_to_app_.write(buffer, size, timeout);
    triger_event(ChannelType::LOW_UP);
    if (ret < 0) {
        #ifdef DEBUG
        std::cout << "tcp -> CircularBuffer fail and retry" << std::endl;
        #endif
        //retry 1 time
        ret = tcp_to_app_.write(buffer, size, timeout);
        if (ret < 0 ) {
            std::cerr << "tcp -> CircularBuffer fail" << std::endl;
        }
        triger_event(ChannelType::LOW_UP);
    } 
    return ret;
}

int Transport::read(uint8_t* data, uint32_t& msg_id, size_t size, std::chrono::milliseconds timeout) {
    while (true) {
        // 检查并处理已完成的消息
        for (auto it = message_cache.begin(); it != message_cache.end();) {
            auto& buffer = it->second;
            int readSize = buffer.total_size;
            if (!buffer.is_complete) {
                ++it;
                continue;
            }
            // 重组消息
            std::vector<uint8_t> reconstructed_data;
            reconstructed_data.reserve(buffer.total_size);
            for (const auto& [segment_id, segment_data] : buffer.segments) {
                reconstructed_data.insert(reconstructed_data.end(), segment_data.begin(), segment_data.end());
            }
        
            msg_id = it->first;  // 记录消息 ID
            
            if (buffer.is_compressed) {
                readSize = decompressData(reconstructed_data, data, size);
                if (readSize <= 0) {
                    std::cerr << "Decompression failed! Error code: " << readSize << std::endl;
                    it = message_cache.erase(it);  // 清理损坏的消息
                    continue; // 继续处理下一个消息
                }
            } else {
                memcpy(data, reconstructed_data.data(), reconstructed_data.size());
            }
            // 清理已完成的消息
            it = message_cache.erase(it);
            return readSize; // 处理完成后返回
        }
        
        // 读取消息长度
        uint32_t dataLen;
        if (tcp_to_app_.peek(reinterpret_cast<char*>(&dataLen), sizeof(uint32_t), timeout) < 0) {
            return -1; // 超时
        }
        dataLen = ntohl(dataLen);

        if (dataLen > segment_size_ || dataLen <= sizeof(uint32_t)) {
            std::cout << "Wrong Msg size: " << dataLen << " clear the data" << std::endl;
            //tcp_to_app_.read(reinterpret_cast<char*>(&dataLen), sizeof(uint32_t), timeout); // 跳过无效数据
            tcp_to_app_.clear();
            return -2;
        }
        std::vector<char> temp_buffer(dataLen);
        // 读取完整分包
        if (tcp_to_app_.read(temp_buffer.data(), dataLen, timeout) < 0) {
            std::cout << "Read timeout\n";
            return -1; // 超时
        }

        // 反序列化消息
        Msg msg = deserializeMsg(temp_buffer);

        //crc check
        uint32_t crc = calculateChecksum(msg.payload);
        if (msg.footer.checksum != crc) {
            // Print CRC in hexadecimal format
            std::cout << "crc error: 0x" << std::hex 
                << std::uppercase << std::setfill('0') 
                << std::setw(8) << crc << std::endl;
            return -3;
        }
        //如果对端切换key
        if (msg.header.flag & FLAG_KEY_UPDATE) {
            switchToNewKeys();
        }
        // 如果对端加密，直接把模式改成加密
        if (msg.header.flag & FLAG_ENCRYPTED) {
            std::vector<uint8_t> decrypted_segment;
            if (!decryptData(sessionKey_rx_, msg.payload, decrypted_segment)) {
                std::cerr << "Warning: Decryption failed for msg: " << msg.header.msg_id << std::endl;
                return -4;
            }
            // 只有解密成功，才设置加密模式，防止错误状态
            setEncryptMode(true);
            msg.payload = std::move(decrypted_segment);
        } else {
            setEncryptMode(false);
        }

        // 查找或创建缓存项
        auto& buffer = message_cache[msg.header.msg_id];
        buffer.last_update = std::chrono::steady_clock::now();
        buffer.is_compressed = msg.header.flag & FLAG_COMPRESSED;

        // 累计总大小
        buffer.total_size += msg.payload.size();

        // 检查总大小限制
        if (buffer.total_size > max_message_size_ || buffer.total_size > size) {
            std::cerr << "Message too large, msg_id: " << msg.header.msg_id 
                << " size: " << buffer.total_size 
                << " buffer size: " << size
                << " max size: " << max_message_size_
                << std::endl;
            message_cache.erase(msg.header.msg_id); // 丢弃超大消息
            continue;
        }
                
        buffer.segments[msg.header.segment_id] = msg.payload;
        
        if ((msg.header.flag & FLAG_SEGMENTED) == 0) {
            buffer.total_segments = msg.header.segment_id + 1;
        }

        // 检查是否完成
        if (buffer.segments.size() == buffer.total_segments) {
            buffer.is_complete = true;
        }

        // 检查缓存大小限制
        if (message_cache.size() > max_cache_size_) {
            std::cout << "Cache size exceeded, removing oldest entry" << std::endl;
            auto oldest = std::min_element(
                message_cache.begin(),
                message_cache.end(),
                [](const auto& a, const auto& b) {
                    return a.second.last_update < b.second.last_update;
                });
            message_cache.erase(oldest);
        }

        // 超时清理未完成消息
        auto now = std::chrono::steady_clock::now();
        for (auto it = message_cache.begin(); it != message_cache.end();) {
            if (now - it->second.last_update > timeout) {
                std::cout << "Message timeout, msg_id: " << it->first << std::endl;
                it = message_cache.erase(it);
            } else {
                ++it;
            }
        }
    }
}
