#ifndef FieldValue_HPP
#define FieldValue_HPP

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

class Document;  // 前向声明
template <typename T>
struct always_false : std::false_type {};
// 定义FieldValue，可以支持不同的数据类型，包括 Document
using FieldValue = std::variant<std::monostate, int, double, bool, std::string, std::time_t, std::vector<uint8_t>, std::shared_ptr<Document>>;
//using FieldValue = std::variant<std::monostate,int, double, bool, std::string, std::time_t, std::vector<uint8_t>>;

template <typename T>
bool compare(const T& value, const FieldValue& queryValue, const std::string& op) {
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
std::function<bool(const FieldValue&)> createPredicate(const T& queryValue, const std::string& op) {
    return [queryValue, op](const FieldValue& fieldValue) -> bool {
        const auto& value = std::get<T>(fieldValue);  // 取出FieldValue中的实际值
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

#endif