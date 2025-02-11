#include <iostream>
#include <chrono>
#include <random>
#include <sstream>
#include <thread>
#include <iomanip>
#include <malloc.h>
#include "util.hpp"

static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// 编码函数
std::string encodeBase64(const std::vector<uint8_t>& data) {
    std::string result;
    int val = 0;
    int valb = -6;
    for (unsigned char c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            result.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) result.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (result.size() % 4) result.push_back('=');
    return result;
}

// 解码函数
std::vector<uint8_t> decodeBase64(const std::string& encoded_string) {
    std::vector<uint8_t> result;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[base64_chars[i]] = i;
    int val = 0;
    int valb = -8;
    for (unsigned char c : encoded_string) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            result.push_back((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    return result;
}

void print_memory_usage() {
    struct mallinfo2 info = mallinfo2();
    std::cout << "Total allocated (arena): " << info.arena << " bytes\n";
    std::cout << "Total allocated (in-use): " << info.uordblks << " bytes\n";
    std::cout << "Total free (unused): " << info.fordblks << " bytes\n";
}

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

// 打印密钥的十六进制表示
void printHex(const std::vector<unsigned char>& data) {
    for (unsigned char c : data) {
        printf("%02X ", c);
    }
    printf("\n");
}

std::string toHexString(const std::vector<unsigned char>& vec) {
    std::ostringstream oss;
    for (unsigned char c : vec) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    }
    return oss.str();
}

std::string get_timestamp_sec() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    std::time_t seconds = millis / 1000;
    std::tm tm = *std::localtime(&seconds);
    std::stringstream ss;

    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    
    return ss.str();
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
    // 确保字符串以"${"开头并以"}"结尾
    if (dateStr.size() < 4 || dateStr.substr(0, 2) != "${" || dateStr.back() != '}') {
        return false; // 如果不符合格式，直接返回false
    }

    // 去除前后的"${"和"}"
    std::string trimmedDate = dateStr.substr(2, dateStr.length() - 3);

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
        std::istringstream stream(trimmedDate);
        std::tm tm = {};
        stream >> std::get_time(&tm, format.c_str());

        // 确保流没有发生错误并且已经完全读取完日期字符串
        if (!stream.fail() && stream.eof()) {
            return true; // 成功解析并且符合格式
        }
    }
    return false; // 全部格式解析失败
}

std::time_t stringToTimeT(const std::string& dateTimeStr) {
    // 确保字符串以"${"开头并以"}"结尾
    if (dateTimeStr.size() < 4 || dateTimeStr.substr(0, 2) != "${" || dateTimeStr.back() != '}') {
        throw std::runtime_error("Invalid date format, must be enclosed in ${}.");
    }

    // 去除前后的"${"和"}"
    std::string trimmedDate = dateTimeStr.substr(2, dateTimeStr.length() - 3);

    // 支持的时间格式
    std::vector<std::string> formats = {
        "%b %d %H:%M:%S %Y",
        "%a %b %d %H:%M:%S %Y",
        "%Y-%m-%d %H:%M:%S",
        "%m/%d/%Y %H:%M:%S",
        "%Y/%m/%d %H:%M:%S"
    };

    std::tm tm = {};
    std::stringstream ss;

    // 尝试解析日期
    for (const auto& format : formats) {
        ss.clear();  // 清除错误标志
        ss.str(trimmedDate);  // 重置字符串流为当前的输入字符串

        ss >> std::get_time(&tm, format.c_str());
        if (!ss.fail()) {
            return std::mktime(&tm);  // 成功解析
        }
    }

    throw std::runtime_error("Failed to parse date/time string: " + dateTimeStr);
}

std::string generateUniqueId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);

    unsigned char uuid[16];
    for (int i = 0; i < 16; ++i) uuid[i] = dist(gen);

    // 设置 UUID 版本号 (v4) 和 variant
    uuid[6] = (uuid[6] & 0x0F) | 0x40;  // 设置 UUID v4
    uuid[8] = (uuid[8] & 0x3F) | 0x80;  // 设置 variant

    std::ostringstream oss;
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) oss << "-";
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)uuid[i];
    }
    return oss.str();
}
