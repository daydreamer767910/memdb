#ifndef UTIL_HPP
#define UTIL_HPP

#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

template <typename T>
T get_env_var(const std::string& env_var, T default_value) {
    const char* value = std::getenv(env_var.c_str());
    if (value) {
        try {
            // 使用 std::istringstream 将字符串转换为目标类型 T
            std::istringstream iss(value);
            T result;
            iss >> result;

            if (iss.fail()) {
                throw std::invalid_argument("Failed to convert value");
            }
            return result;
        } catch (const std::exception& e) {
            std::cerr << "Invalid value for environment variable " << env_var << ": " << value << std::endl;
            return default_value;
        }
    }
    return default_value;  // 如果环境变量没有设置，返回默认值
}

using json = nlohmann::json;
std::string encodeBase64(const std::vector<uint8_t>& data);
std::vector<uint8_t> decodeBase64(const std::string& encoded_string);

void print_memory_usage();
void printHex(const std::vector<unsigned char>& data);
void print_packet(const uint8_t* packet, size_t length);
void print_packet(const std::vector<uint8_t>& packet);
void print_packet(const std::vector<char>& packet);
std::string toHexString(const std::vector<unsigned char>& vec);
std::vector<uint8_t> hexStringToBytes(const std::string& hex);
bool isDate(const std::string& dateStr);
std::string get_timestamp();
std::string get_timestamp_sec();
std::time_t stringToTimeT(const std::string& dateTimeStr);
std::string generateUniqueId();

#endif