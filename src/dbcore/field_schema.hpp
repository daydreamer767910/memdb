#include <iostream>
#include <string>
#include <unordered_map>
#include <variant>
#include <regex>
#include <vector>
#include <memory>
#include <optional>
#include "fieldvalue.hpp"
#include "document.hpp"

class FieldSchema {
public:
    struct Constraints {
		bool required = false;
		uint64_t depth = 0xffffffffffffffff; // bits for depths, eg. 0x01 means depth 0 is effective
		//std::optional<FieldValue> defaultValue = std::nullopt;
		std::optional<int> minLength;              // 最小长度（字符串）
		std::optional<int> maxLength;              // 最大长度（字符串）
		std::optional<int> minValue;               // 最小值（数值）
		std::optional<int> maxValue;               // 最大值（数值）
		std::optional<std::string> regexPattern;    // 字符串格式限制

		void fromJson(const json& j);

		json toJson() const ;
	};


private:
    FieldType type_;
    Constraints constraints_;
public:
	
	// Default constructor
    FieldSchema() : type_(FieldType::NONE) {}

    // 获取字段类型
    FieldType getType() const { return type_; }

    // 获取字段约束
    const Constraints& getConstraints() const { return constraints_; }

	void fromJson(const json& j) {
        type_ = typefromJson(j);

        // Parse constraints
        if (j.contains("constraints")) {
            constraints_.fromJson(j.at("constraints"));
        }
    }

	json toJson() const {
        json j = typetoJson(type_);
        j["constraints"] = constraints_.toJson();
        return j;
    }
    // 验证字段值是否合法
    bool validate(const Field& field, uint8_t depth) const;

};
