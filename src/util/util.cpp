#include <iostream>
#include <chrono>
#include <random>
#include <sstream>
#include <thread>
#include <iomanip>
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

bool isDate(const std::string& dateStr) {
    // 支持的时间格式
    std::vector<std::string> formats = {
        "%b %d %H:%M:%S %Y",
        "%a %b %d %H:%M:%S %Y",
        "%Y-%m-%d %H:%M:%S",
        "%m/%d/%Y %H:%M:%S",
        "%Y/%m/%d %H:%M:%S"
    };

    // 尝试解析日期
    for (const auto& format : formats) {
        std::istringstream stream(dateStr);
        std::tm tm = {};
        stream >> std::get_time(&tm, format.c_str());
        if (!stream.fail()) {
            return true; // 成功解析
        }
    }
    return false; // 全部格式解析失败
}

std::time_t stringToTimeT(const std::string& dateTimeStr) {
    std::vector<std::string> formats = {
        "%b %d %H:%M:%S %Y",
        "%a %b %d %H:%M:%S %Y",
        "%Y-%m-%d %H:%M:%S",
        "%m/%d/%Y %H:%M:%S",
        "%Y/%m/%d %H:%M:%S"
    };
    
    std::tm tm = {};
    std::stringstream ss;
    
    for (const auto& format : formats) {
        ss.clear();  // Clear the error flag
        ss.str(dateTimeStr);  // Reset stringstream with the current input string
        
        ss >> std::get_time(&tm, format.c_str());
        if (!ss.fail()) {
            return std::mktime(&tm);  // Successfully parsed
        }
    }

    throw std::runtime_error("Failed to parse date/time string: " + dateTimeStr);
}

std::string generateUniqueId() {
    // 获取当前时间戳（毫秒级）
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    // 获取当前线程的ID
    std::thread::id threadId = std::this_thread::get_id();
    
    // 使用 std::hash 获取线程ID的哈希值
    std::hash<std::thread::id> hasher;
    size_t threadIdHash = hasher(threadId);
    
    // 生成随机数
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);  // 随机数范围
    
    int randomValue = dis(gen);
    
    // 组合时间戳、线程ID的哈希值和随机数生成唯一ID
    std::stringstream idStream;
    idStream << std::setw(8) << std::setfill('0') << (millis & 0xFFFFFFFFFFFFFFFF) << "-" 
             << std::setw(4) << std::setfill('0') << (threadIdHash & 0xFFFFFFFF) << "-" 
             << randomValue;
    
    return idStream.str();
}
