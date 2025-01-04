#include "transport.hpp"

// 计算校验和
uint32_t Transport::calculateChecksum(const std::vector<char>& data) {
	uint32_t checksum = 0;
	for (char c : data) {
		checksum += static_cast<uint8_t>(c);
	}
	return checksum;
}

// 序列化 Msg 为网络字节序
std::vector<char> Transport::serializeMsg(const Msg& msg) {
	std::vector<char> buffer(sizeof(MsgHeader) + msg.payload.size() + sizeof(MsgFooter));
	size_t offset = 0;

	std::memcpy(buffer.data() + offset, &msg.header, sizeof(MsgHeader));
	offset += sizeof(MsgHeader);

	std::memcpy(buffer.data() + offset, msg.payload.data(), msg.payload.size());
	offset += msg.payload.size();

	std::memcpy(buffer.data() + offset, &msg.footer, sizeof(MsgFooter));
	return buffer;
}

// 反序列化网络字节序为 Msg
Msg Transport::deserializeMsg(const std::vector<char>& buffer) {
	if (buffer.size() < sizeof(MsgHeader) + sizeof(MsgFooter)) {
		throw std::runtime_error("Invalid Msg size");
	}

	Msg msg;
	size_t offset = 0;

	std::memcpy(&msg.header, buffer.data() + offset, sizeof(MsgHeader));
	offset += sizeof(MsgHeader);

	msg.payload.assign(buffer.begin() + offset, buffer.end() - sizeof(MsgFooter));
	offset += msg.payload.size();

	std::memcpy(&msg.footer, buffer.data() + offset, sizeof(MsgFooter));
	return msg;
}