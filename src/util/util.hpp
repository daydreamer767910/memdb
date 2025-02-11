#ifndef UTIL_HPP
#define UTIL_HPP

#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

using json = nlohmann::json;
std::string encodeBase64(const std::vector<uint8_t>& data);
std::vector<uint8_t> decodeBase64(const std::string& encoded_string);

void print_memory_usage();
void printHex(const std::vector<unsigned char>& data);
void print_packet(const uint8_t* packet, size_t length);
void print_packet(const std::vector<uint8_t>& packet);
void print_packet(const std::vector<char>& packet);
std::string toHexString(const std::vector<unsigned char>& vec);
bool isDate(const std::string& dateStr);
std::string get_timestamp();
std::string get_timestamp_sec();
std::time_t stringToTimeT(const std::string& dateTimeStr);
std::string generateUniqueId();

#endif