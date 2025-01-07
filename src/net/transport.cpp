#include <cstring>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <netinet/in.h> // htons, htonl, ntohs, ntohl
#include "transport.hpp"
#include "util/util.hpp"

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


int Transport::appSend(const json& json_data, uint32_t msg_id, std::chrono::milliseconds timeout) {
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
		//print_packet(reinterpret_cast<const uint8_t*>(network_data.data()), network_data.size());
		offset += chunk_size;
	}
	return total_size;
}

int Transport::tcpReadFromApp(char* buffer, size_t size, std::chrono::milliseconds timeout) {
	//读header的length字段
	size_t dataLen = app_to_tcp_.readableSize();
	//std::cout << "tcpReadFromApp:readableSize:" << dataLen << std::endl;
	if (dataLen > 0) {
		size = std::min(dataLen,size);
	}
	return app_to_tcp_.read(buffer, size, timeout);
}


int Transport::tcpReceive(const char* buffer, size_t size, std::chrono::milliseconds timeout) {
	//std::cout << "tcpReceive: \n";
	//print_packet(reinterpret_cast<const uint8_t*>(buffer),size);
	return tcp_to_app_.write(buffer, size, timeout);
}

int Transport::appReceive(json& json_data, std::chrono::milliseconds timeout) {
	//先假读MsgHeader计算出数据长度，然后根据读出来的长度获取数据
	std::vector<char> temp_buffer(segment_size_);
	std::string reconstructed_data;

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

		reconstructed_data.append(msg.payload.begin(), msg.payload.end());

		if (msg.header.flag == 0) { // 最后一段
			break;
		}
	}

	json_data = json::parse(reconstructed_data);
	return true;
}
