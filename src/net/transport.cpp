#include <cstring>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <netinet/in.h> // htons, htonl, ntohs, ntohl
#include "transport.hpp"
#include "util/util.hpp"

Transport::Transport(size_t buffer_size, uv_loop_t* loop)
	: app_to_tcp_(buffer_size), 
	tcp_to_app_(buffer_size), loop_(loop) {
	if (pipe(pipe_fds) == -1) {
		std::cerr << "pipe err" << std::endl;
	}
	int flags = fcntl(pipe_fds[0], F_GETFL, 0);
	fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK);
	uv_poll_init(loop_,&poll_handle,pipe_fds[0]);
	uv_poll_start(&poll_handle, UV_READABLE, Transport::process_event);
	poll_handle.data = this;
	output_callback_ = nullptr;
	buffer_callback_ = nullptr;
	size_callback_ = 0;
}

Transport::~Transport() {
	uv_poll_stop(&poll_handle);
	close(pipe_fds[0]);
	close(pipe_fds[1]);
}

void Transport::process_event(uv_poll_t* handle, int status, int events) {
	if (status <0 ) {
		std::cerr << "Poll error:" << uv_strerror(status) << std::endl;
		return ;
	}
	auto transport = reinterpret_cast<Transport*>(handle->data);
	if (events & UV_READABLE) {
		char signal;
		if (transport)
			::read(transport->pipe_fds[0], &signal, 1);
		//callback...
		if (signal == '1')
			transport->on_send();
	}
}

void Transport::on_send() {
	//std::cout << "send signal\n";
	if (output_callback_ && buffer_callback_ && size_callback_ > 0) {
		while(true) {
			int len = this->output(buffer_callback_,size_callback_,std::chrono::milliseconds(0));
			if (len <=0 )
				break;
			output_callback_(buffer_callback_,len);
		}
	}
}

void Transport::triger_on_send() {
	// triger on_send 写入管道通知事件循环
	const char signal = '1';
	write(pipe_fds[1], &signal, sizeof(signal));
}

// 计算校验和
uint32_t Transport::calculateChecksum(const std::vector<char>& data) {
    uint32_t checksum = 0;
    for (char c : data) {
        checksum += static_cast<uint8_t>(c); // 避免符号扩展
    }
    return checksum;
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


int Transport::send(const json& json_data, uint32_t msg_id, std::chrono::milliseconds timeout) {
	std::string serialized = json_data.dump();
	size_t total_size = serialized.size();
	size_t segment_id = 0;
	//std::cout << "appSend:" << serialized << std::endl;
	size_t offset = 0;
	while (offset < total_size) {
		size_t chunk_size = std::min(total_size - offset, segment_size_-sizeof(MsgHeader) - sizeof(MsgFooter));

		Msg msg;
		msg.header.length = chunk_size + sizeof(MsgHeader) + sizeof(MsgFooter);
		msg.header.msg_id = msg_id;
		msg.header.segment_id = segment_id++;
		if ((total_size - offset) <= (segment_size_-sizeof(MsgHeader) - sizeof(MsgFooter))) {
			msg.header.flag = 0; //last segment
		} else {
			msg.header.flag = 1;
		}
		

		msg.payload.assign(serialized.begin() + offset, serialized.begin() + offset + chunk_size);
		msg.footer.checksum = calculateChecksum(msg.payload);

		std::vector<char> network_data = serializeMsg(msg);
		if (app_to_tcp_.write(network_data.data(), network_data.size(), timeout)<0) {
			return -1; // 写入超时
		}
		triger_on_send();
		
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
	//std::cout << "tcpReceive: \n";
	//print_packet(reinterpret_cast<const uint8_t*>(buffer),size);
	return tcp_to_app_.write(buffer, size, timeout);
}

int Transport::read(json& json_data, std::chrono::milliseconds timeout) {
	//先假读MsgHeader计算出数据长度，然后根据读出来的长度获取数据
	std::vector<char> temp_buffer(segment_size_);
	std::string reconstructed_data;
	uint32_t msg_id;
	while (true) {
		//读header的length字段
		uint32_t dataLen;
		if (tcp_to_app_.peek(reinterpret_cast<char*>(&dataLen), sizeof(uint32_t), timeout) < 0) {
			return -1; // 超时
		}
		dataLen = ntohl(dataLen);
		if (dataLen > segment_size_ || dataLen <= sizeof(uint32_t)) {
			//somthing is wrong with the packet
			std::cout << "wrong Msg size:" << dataLen << " skip the data" << std::endl;
			return -2;
		}

		if (tcp_to_app_.read(temp_buffer.data(), dataLen, timeout)<0) {
			std::cout << "read timeout\n";
			return -1; // 读取超时
		}

		Msg msg = deserializeMsg(temp_buffer);
		msg_id = msg.header.msg_id;
		reconstructed_data.append(msg.payload.begin(), msg.payload.end());

		if (msg.header.flag == 0) { // 最后一段
			break;
		}
	}

	json_data = json::parse(reconstructed_data);
	return true;
}

int Transport::read_all(std::vector<json>& json_datas,  // 存储已完成的消息
    std::chrono::milliseconds timeout) 
{
    std::vector<char> temp_buffer(segment_size_);
	
    while (true) {
        // 检查并处理已完成的消息
        for (auto it = message_cache.begin(); it != message_cache.end();) {
            auto& buffer = it->second;
            if (buffer.is_complete) {
                // 重组消息
                std::string reconstructed_data;
                for (const auto& [segment_id, segment_data] : buffer.segments) {
                    reconstructed_data.append(segment_data.begin(),segment_data.end());
                }
				try {
					// 解析 JSON 并加入结果
                	json_datas.push_back(json::parse(reconstructed_data));
				} catch (...) {
					std::cout << "json parse fail:\n" << reconstructed_data << std::endl;
				}
                
                // 清理已完成的消息
                it = message_cache.erase(it);
            } else {
                ++it;
            }
        }

        // 如果有完成的消息，退出循环
        if (!json_datas.empty()) {
            return json_datas.size(); // 返回成功条数
        }

        // 读取消息长度
        uint32_t dataLen;
        if (tcp_to_app_.peek(reinterpret_cast<char*>(&dataLen), sizeof(uint32_t), timeout) < 0) {
            return -1; // 超时
        }
        dataLen = ntohl(dataLen);

        if (dataLen > segment_size_ || dataLen <= sizeof(uint32_t)) {
            std::cout << "Wrong Msg size: " << dataLen << " skip the data" << std::endl;
            tcp_to_app_.read(reinterpret_cast<char*>(&dataLen), sizeof(uint32_t), timeout); // 跳过无效数据
            continue;
        }

        // 读取完整分包
        if (tcp_to_app_.read(temp_buffer.data(), dataLen, timeout) < 0) {
            std::cout << "Read timeout\n";
            return -1; // 超时
        }

        // 反序列化消息
        Msg msg = deserializeMsg(temp_buffer);

        // 查找或创建缓存项
        auto& buffer = message_cache[msg.header.msg_id];
        buffer.last_update = std::chrono::steady_clock::now();

        // 累计总大小
        buffer.total_size += msg.payload.size();

        // 检查总大小限制
        if (buffer.total_size > max_message_size_) {
            std::cout << "Message too large, msg_id: " << msg.header.msg_id << " size: " << buffer.total_size << std::endl;
            message_cache.erase(msg.header.msg_id); // 丢弃超大消息
            continue;
        }

        // 存储分包
        buffer.segments[msg.header.segment_id] = msg.payload;
        if (msg.header.flag == 0) {
            buffer.total_segments = msg.header.segment_id + 1;
        }

        // 检查是否完成
        if (buffer.segments.size() == buffer.total_segments) {
            buffer.is_complete = true;
        }

        // 检查缓存大小限制
        if (message_cache.size() > max_cache_size) {
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
