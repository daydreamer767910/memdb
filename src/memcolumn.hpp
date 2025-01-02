#ifndef MEMCOLUMN_HPP
#define MEMCOLUMN_HPP

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <stdexcept>
#include <set>
#include <fstream>
#include <nlohmann/json.hpp>

using Field = std::variant<int, double, bool, std::string, std::time_t, std::vector<uint8_t>>;

struct Column {
    std::string name = "";
    std::string type = "";
    bool nullable = false;
    Field defaultValue = nullptr;
    bool primaryKey = false;
};

using Row = std::unordered_map<std::string, Field>;

// Define an index type
using Index = std::map<Field, std::set<int>>;

// Field 转 JSON 的函数
nlohmann::json fieldToJson(const Field& field);
Field jsonToField(const std::string& type,const nlohmann::json& value);

std::ostream& operator<<(std::ostream& os, const Field& field);
Field getDefault(const std::string& type);

bool isValidType(const Field& value, const std::string& type);
std::vector<Column> jsonToColumns(const nlohmann::json& jsonColumns);

#endif