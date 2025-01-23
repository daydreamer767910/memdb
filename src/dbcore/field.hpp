#ifndef FIELD_HPP
#define FIELD_HPP
#include <unordered_map>
#include <string>
#include <iostream>
#include <stdexcept>
#include <variant>
#include <memory>
#include <optional>
#include <set>
#include <fstream>
#include <nlohmann/json.hpp>
#include "util/util.hpp"
#include "fieldvalue.hpp"

class Field {
public:
    Field() = default;

    Field(const FieldValue& value) : value_(value) {}

    // 获取 Field 的值
    template <typename T>
    T getValue() const {
        return std::get<T>(value_);
    }

	const FieldValue& getValue() const {
		return value_;
	}

    // 设置 Field 的值
    void setValue(const FieldValue& value) {
        value_ = value;
    }

    struct Hash {
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
				//} else if constexpr (std::is_same_v<T, std::shared_ptr<Document>>) {
					//return std::hash<std::shared_ptr<Document>>{}(value);
				} else {
					static_assert(always_false<T>::value, "Unsupported type in FieldValue");
				}
			}, field.value_);
		}
    };

    // 比较操作符
    bool operator==(const Field& other) const {
        return value_ == other.value_;
    }

    bool operator<(const Field& other) const {
        return value_ < other.value_;
    }

	std::ostream& operator<<(std::ostream& os) const {
        os << toJson().dump(); // 使用 JSON 的 dump 方法输出为字符串
        return os;
    }

	// 在头文件中定义 operator<<
    friend std::ostream& operator<<(std::ostream& os, const Field& field) {
        os << field.toJson().dump(); // 使用 JSON 的 dump 方法输出为字符串
        return os;
    }

	json toJson() const;
	Field& fromJson(const std::string& type,const json& value);

	Field& fromBinary(const char* data, size_t size);
	std::string toBinary() const;

private:
    FieldValue value_;
};

#endif