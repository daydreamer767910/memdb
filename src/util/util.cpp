#include "util.hpp"

/*
| Message Length (4 bytes) | Message Type (4 bytes) | Payload Data (variable length) |
*/
void print_packet(const uint8_t* packet, size_t length) {
    std::vector<unsigned char> packet_unsigned(packet, packet+length);
    print_packet(packet_unsigned);
}

void print_packet(const std::vector<uint8_t>& packet) {
    //std::cout << "Packed data: ";
    for (size_t i = 0; i < packet.size(); ++i) {
        unsigned char byte = packet[i];

        // 打印十六进制表示
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";

        // 每16字节换行
        if ((i + 1) % 16 == 0) {
            std::cout << "  ";
            for (size_t j = i - 15; j <= i; ++j) {
                std::cout << (std::isprint(packet[j]) ? (char)packet[j] : '.');
            }
            std::cout << std::endl;
        }
    }

    // 如果没有刚好16字节一行的部分，处理最后的字符显示
    if (packet.size() % 16 != 0) {
        size_t pad = 16 - (packet.size() % 16);
        for (size_t i = 0; i < pad; ++i) {
            std::cout << "   ";
        }
        std::cout << "  ";
        for (size_t i = packet.size() - packet.size() % 16; i < packet.size(); ++i) {
            std::cout << (std::isprint(packet[i]) ? (char)packet[i] : '.');
        }
        std::cout << std::endl;
    }
}

void print_packet(const std::vector<char>& packet) {
    std::vector<unsigned char> packet_unsigned(packet.begin(), packet.end());
    print_packet(packet_unsigned);
}

std::vector<uint8_t> pack_data(const json& payload, uint32_t message_type) {
    std::vector<uint8_t> packet;

    // 序列化 JSON 数据
    std::string json_str = payload.dump();

    // 消息长度 = 消息类型字段长度 + 消息体长度
    uint32_t length = 4 + json_str.size();

    // 将消息长度写入数据包
    packet.push_back(length >> 24);
    packet.push_back(length >> 16);
    packet.push_back(length >> 8);
    packet.push_back(length);

    // 将消息类型写入数据包
    packet.push_back(message_type >> 24);
    packet.push_back(message_type >> 16);
    packet.push_back(message_type >> 8);
    packet.push_back(message_type);

    // 将 JSON 字符串加入数据包
    packet.insert(packet.end(), json_str.begin(), json_str.end());

	// 打印打包后的数据
    //print_packet(packet);

    return packet;
}

std::pair<uint32_t, json> unpack_data(const std::vector<uint8_t>& packet) {
    // 确保数据包长度足够
    if (packet.size() < 8) {
        throw std::runtime_error("Invalid packet: too short");
    }

    // 解析消息长度（前 4 字节，不包括自身长度）
    //uint32_t length = (packet[0] << 24) | (packet[1] << 16) | (packet[2] << 8) | packet[3];

    // 解析消息类型（接着 4 字节）
    uint32_t message_type = (packet[4] << 24) | (packet[5] << 16) | (packet[6] << 8) | packet[7];

    // 提取消息体
    std::string json_str(packet.begin() + 8, packet.end());

    // 反序列化 JSON 数据
    json payload = json::parse(json_str);

	// 打印拆包结果
    std::cout << "Message Type: " << message_type << std::endl;
    std::cout << "Payload: " << payload.dump(4) << std::endl;

    return {message_type, payload};
}

std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    std::time_t seconds = millis / 1000;
    std::tm tm = *std::localtime(&seconds);
    std::stringstream ss;

    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setw(3) << std::setfill('0') << (millis % 1000);  // 毫秒部分

    return ss.str();
}


std::time_t stringToTimeT(const std::string& dateTimeStr) {
	std::vector<std::string> formats = {
        "%b %d %H:%M:%S %Y",  // "Dec 28 13:15:37 2024"
        "%a %b %d %H:%M:%S %Y",  // "Sat Dec 28 13:15:37 2024"
        "%Y-%m-%d %H:%M:%S",  // "2024-12-28 13:15:37"
        "%m/%d/%Y %H:%M:%S",  // "12/28/2024 13:15:37"
        "%Y/%m/%d %H:%M:%S"   // "2024/12/28 13:15:37"
    };
    std::tm tm = {};
	for (const auto& format : formats) {
        std::stringstream ss(dateTimeStr);
        ss >> std::get_time(&tm, format.c_str());
        if (!ss.fail()) {
            return std::mktime(&tm);  // Successfully parsed
        }
    }
	throw std::runtime_error("Failed to parse date/time string: " + dateTimeStr);
	/*
    std::istringstream ss(dateTimeStr);
    ss >> std::get_time(&tm, format.c_str());
    if (ss.fail()) {
        throw std::runtime_error("Failed to parse date/time string: " + dateTimeStr);
    }
    return std::mktime(&tm);*/
}