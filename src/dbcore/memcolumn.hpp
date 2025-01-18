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
#include "util/util.hpp"

// 定义占位符模板
template <typename T>
struct always_false : std::false_type {};

using Field = std::variant<std::monostate,int, double, bool, std::string, std::time_t, std::vector<uint8_t>>;

struct Column {
    std::string name = "";
    std::string type = "";
    bool nullable = false;
    Field defaultValue = std::monostate{};;
    bool primaryKey = false;
};

using Document = std::unordered_map<std::string, Field>;
using Row = std::vector<Field>;
// Define an index type
using Index = std::map<Field, std::set<size_t>>;

// 自定义哈希函数
struct FieldHash {
    std::size_t operator()(const Field& field) const {
        return std::visit([](const auto& value) -> std::size_t {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                // 对 std::monostate 进行处理，返回一个固定的哈希值
                return 0;
            } else if constexpr (std::is_same_v<T, int>) {
                return std::hash<int>{}(value);
            } else if constexpr (std::is_same_v<T, double>) {
                return std::hash<double>{}(value);
            } else if constexpr (std::is_same_v<T, bool>) {
                return std::hash<bool>{}(value);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return std::hash<std::string>{}(value);
            } else if constexpr (std::is_same_v<T, std::time_t>) {
                return std::hash<std::time_t>{}(value);
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                // 对 vector<uint8_t> 的哈希
                std::size_t seed = 0;
                for (auto byte : value) {
                    seed ^= std::hash<uint8_t>{}(byte) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
                }
                return seed;
            } else {
                static_assert(always_false<T>::value, "Unsupported type in Field");
            }
        }, field);
    }
};

// 相等比较（std::variant 本身支持 ==，无需额外实现）
struct FieldEqual {
    bool operator()(const Field& lhs, const Field& rhs) const {
        return lhs == rhs;
    }
};

// 定义主键索引
using PrimaryKeyIndex = std::unordered_map<Field, size_t, FieldHash, FieldEqual>;

template <typename T>
bool compare(const T& value, const Field& queryValue, const std::string& op) {
    // 处理具体的比较操作符
    if (op == "==") {
        return value == std::get<T>(queryValue);
    } else if (op == "<") {
        return value < std::get<T>(queryValue);
    } else if (op == ">") {
        return value > std::get<T>(queryValue);
    } else if (op == "<=") {
        return value <= std::get<T>(queryValue);
    } else if (op == ">=") {
        return value >= std::get<T>(queryValue);
    } else {
        throw std::invalid_argument("Unsupported comparison operator: " + op);
    }
}

template <typename T>
std::function<bool(const Field&)> createPredicate(const T& queryValue, const std::string& op) {
    return [queryValue, op](const Field& fieldValue) -> bool {
        const auto& value = std::get<T>(fieldValue);  // 取出Field中的实际值
        if (op == "==") {
            return value == queryValue;
        } else if (op == "<") {
            return value < queryValue;
        } else if (op == ">") {
            return value > queryValue;
        } else if (op == "<=") {
            return value <= queryValue;
        } else if (op == ">=") {
            return value >= queryValue;
        }
        return false;
    };
}

// Field 转 JSON 的函数
json fieldToJson(const Field& field);
Field jsonToField(const std::string& type,const json& value);

std::ostream& operator<<(std::ostream& os, const Field& field);
Field getDefault(const std::string& type);

bool isValidType(const Field& value, const std::string& type);
std::vector<Column> jsonToColumns(const json& jsonColumns);
json columnsToJson(const std::vector<Column>& columns);

#endif