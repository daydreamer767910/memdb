#include "field_schema.hpp"

void FieldSchema::Constraints::fromJson(const json& j) {
	if (j.contains("required")) required = j.at("required").get<bool>();
	if (j.contains("depth")) depth = j.at("depth").get<uint64_t>();
	if (j.contains("minLength")) minLength = j.at("minLength").get<int>();
	if (j.contains("maxLength")) maxLength = j.at("maxLength").get<int>();
	if (j.contains("minValue")) minValue = j.at("minValue").get<int>();
	if (j.contains("maxValue")) maxValue = j.at("maxValue").get<int>();
	if (j.contains("regexPattern")) regexPattern = j.at("regexPattern").get<std::string>();
}

json FieldSchema::Constraints::toJson() const {
	json j;
	j["required"] = required;
	j["depth"] = depth;

	// Serialize constraints
	if (minLength.has_value()) j["minLength"] = minLength;
	if (maxLength.has_value()) j["maxLength"] = maxLength;
	if (minValue.has_value()) j["minValue"] = minValue;
	if (maxValue.has_value()) j["maxValue"] = maxValue;
	if (regexPattern.has_value()) j["regexPattern"] = regexPattern;

	return j;
}

bool FieldSchema::validate(const Field& field, uint8_t depth) const {
	if ((0x0000000000000001<<depth)&constraints_.depth == 0) return true;
	// 检查类型匹配
	if (!field.typeMatches(type_)) {
		std::cerr << "Field type mismatch, required type: " << type_ << std::endl;
		return false;
	}

	// 如果字段是空值且必需字段未提供值，则不合法
	if (field.is_null() && constraints_.required) {
		std::cerr << "Required field is missing.\n";
		return false;
	}

	try {
        // 检查字段类型
        switch (type_) {
            case FieldType::STRING: {
                auto value = field.getValue<std::string>();
				if (constraints_.minLength.has_value() && value.size() < constraints_.minLength.value()) {
					throw std::invalid_argument("Field value is too short");
				}

				// 检查最大长度
				if (constraints_.maxLength.has_value() && value.size() > constraints_.maxLength.value()) {
					throw std::invalid_argument("Field value is too long");
				}

				// 检查正则表达式模式
				if (constraints_.regexPattern.has_value()) {
					auto regex = std::regex(constraints_.regexPattern.value());
					if (!std::regex_match(value, regex)) {
						throw std::invalid_argument("Field value does not match the required pattern: " + value);
					}
				}
                break;
            }
            case FieldType::INT: {
				auto value = field.getValue<int>();
				// 检查范围
                if (constraints_.minValue && value < constraints_.minValue) {
                    throw std::invalid_argument("value too small");
                }
                if (constraints_.maxValue && value > constraints_.maxValue) {
                    throw std::invalid_argument("value too large");
                }
                break;
			}
            case FieldType::DOUBLE: {
                auto value = field.getValue<double>();
                // 检查范围
                if (constraints_.minValue && value < constraints_.minValue) {
                    throw std::invalid_argument("value too small");
                }
                if (constraints_.maxValue && value > constraints_.maxValue) {
                    throw std::invalid_argument("value too large");
                }
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Validation error: " << e.what() << std::endl;
        return false;
    }
	return true;
}
