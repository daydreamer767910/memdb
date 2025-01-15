#include "util.hpp"

/*
| Message Length (4 bytes) | Message Type (4 bytes) | Payload Data (variable length) |
*/
void print_packet(const uint8_t* packet, size_t length) {
    std::vector<unsigned char> packet_unsigned(packet, packet+length);
    print_packet(packet_unsigned);
}

void print_packet(const std::vector<uint8_t>& packet) {
    for (size_t i = 0; i < packet.size(); ++i) {
        unsigned char byte = packet[i];

        // 打印十六进制表示
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";

        // 每32字节换行
        if ((i + 1) % 32 == 0) {
            std::cout << "  ";
            for (size_t j = i - 31; j <= i; ++j) {
                std::cout << (std::isprint(packet[j]) ? (char)packet[j] : '.');
            }
            std::cout << std::endl;
        }
    }

    // 如果没有刚好32字节一行的部分，处理最后的字符显示
    if (packet.size() % 32 != 0) {
        size_t pad = 32 - (packet.size() % 32);
        for (size_t i = 0; i < pad; ++i) {
            std::cout << "   ";
        }
        std::cout << "  ";
        for (size_t i = packet.size() - packet.size() % 32; i < packet.size(); ++i) {
            std::cout << (std::isprint(packet[i]) ? (char)packet[i] : '.');
        }
        std::cout << std::endl;
    }
}


void print_packet(const std::vector<char>& packet) {
    std::vector<unsigned char> packet_unsigned(packet.begin(), packet.end());
    print_packet(packet_unsigned);
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