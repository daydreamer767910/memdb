#ifndef FieldValue_HPP
#define FieldValue_HPP

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <stdexcept>
#include <regex>
#include <iostream>
#include <memory>
#include <set>
#include <fstream>
#include <nlohmann/json.hpp>
#include "util/util.hpp"

class Document;  // 前向声明


// 定义FieldValue，可以支持不同的数据类型，包括 Document
using FieldValue = std::variant<std::monostate, int, double, bool, std::string, std::time_t, std::vector<uint8_t>, std::shared_ptr<Document>>;
enum class FieldType {
    NONE,
    INT,
    DOUBLE,
    BOOL,
    STRING,
    TIME,
    BINARY,
    DOCUMENT
};


FieldType typefromJson(const json& j);
json typetoJson(const FieldType& type);
std::string typetoString(const FieldType& type);
FieldType typefromString(const std::string& typeStr);
FieldValue getDefault(const FieldType& type);

//FieldValue valuefromJson(const FieldType& type, const json& value);
FieldValue valuefromJson(const json& value);
json valuetoJson(const FieldValue& value);
FieldType getValueType(const FieldValue& value) ;

namespace field_ns {
    bool operator<(const FieldValue& lhs, const FieldValue& rhs);
    bool operator<=(const FieldValue& lhs, const FieldValue& rhs);
    bool operator>=(const FieldValue& lhs, const FieldValue& rhs);
    bool operator>(const FieldValue& lhs, const FieldValue& rhs);
    bool operator==(const FieldValue& lhs, const FieldValue& rhs);
    bool operator!=(const FieldValue& lhs, const FieldValue& rhs);
}

bool compare(const FieldValue& lhs, const FieldValue& rhs, const std::string& op);

// 定义输出运算符
std::ostream& operator<<(std::ostream& os, const FieldType& type);
std::ostream& operator<<(std::ostream& os, const FieldValue& value);
template <typename T>
struct always_false : std::false_type {};

enum class MatchType {
    Contains,
    Prefix,
    Suffix,
    Exact,
    Invalid
};

bool likeMatch(const FieldValue& fieldValue, const FieldValue& queryValue, const std::string& op);

#endif