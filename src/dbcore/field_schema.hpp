#include <iostream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <memory>
#include <optional>
#include "fieldvalue.hpp"
#include "document.hpp"

class FieldSchema {
public:
    
    struct Constraints {
        bool required;
        bool unique;
		uint64_t depth;
        std::optional<FieldValue> defaultValue;

        // Default constructor
        Constraints() : required(false), unique(false), depth(0xffffffffffffffff), defaultValue(std::nullopt) {};
		void fromJson(const json& j) {
            if (j.contains("required")) required = j.at("required").get<bool>();
            if (j.contains("unique")) unique = j.at("unique").get<bool>();
			if (j.contains("depth")) depth = j.at("depth").get<uint64_t>();
            if (j.contains("defaultValue")) {
                // Handle defaultValue parsing based on JSON data type
                if (j.at("defaultValue").is_number_integer()) {
                    defaultValue = j.at("defaultValue").get<int>();
                } else if (j.at("defaultValue").is_string()) {
                    defaultValue = j.at("defaultValue").get<std::string>();
                }
                // Add more cases as needed for other FieldValue types
            }
        };
		json toJson() const {
            json j;
            j["required"] = required;
            j["unique"] = unique;
			j["depth"] = depth;
            if (defaultValue.has_value()) {
                // Convert the variant to JSON based on its actual type
                if (std::holds_alternative<int>(*defaultValue)) {
                    j["defaultValue"] = std::get<int>(*defaultValue);
                } else if (std::holds_alternative<double>(*defaultValue)) {
                    j["defaultValue"] = std::get<double>(*defaultValue);
                } else if (std::holds_alternative<bool>(*defaultValue)) {
                    j["defaultValue"] = std::get<bool>(*defaultValue);
                } else if (std::holds_alternative<std::string>(*defaultValue)) {
                    j["defaultValue"] = std::get<std::string>(*defaultValue);
                }
                // Add more cases as needed for other FieldValue types
            } else {
                j["defaultValue"] = nullptr;
            }

            return j;
        };
    };

private:
    FieldType type_;
    Constraints constraints_;
public:
	
	// Default constructor
    FieldSchema() : type_(FieldType::NONE) {}

    FieldSchema(FieldType type, Constraints constraints = Constraints())
        : type_(type), constraints_(constraints) {}

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
    bool validate(const FieldValue& value, uint8_t depth) const;

private:
    
};
