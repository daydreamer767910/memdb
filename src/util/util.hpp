#ifndef UTIL_HPP
#define UTIL_HPP

#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

using json = nlohmann::json;

void print_packet(const uint8_t* packet, size_t length);
void print_packet(const std::vector<uint8_t>& packet);
void print_packet(const std::vector<char>& packet);
std::vector<uint8_t> pack_data(const json& payload, uint32_t message_type);
std::pair<uint32_t, json> unpack_data(const std::vector<uint8_t>& packet);

std::time_t stringToTimeT(const std::string& dateTimeStr);

#endif