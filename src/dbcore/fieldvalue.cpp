#include "fieldvalue.hpp"


// FieldValue 转 JSON 的函数
json FieldValueToJson(const FieldValue& fieldValue) {
    return std::visit([](auto&& value) -> json {
        using T = std::decay_t<decltype(value)>;
        // 处理 std::monostate
        if constexpr (std::is_same_v<T, std::monostate>) {
            return nullptr;  // 返回 null，表示空值
        } else if constexpr (std::is_same_v<T, std::time_t>) {
			std::string timeStr = std::ctime(&value);
			timeStr.erase(std::remove(timeStr.begin(), timeStr.end(), '\n'), timeStr.end());
			return timeStr;
            //return std::ctime(&value);// 转换为时间戳表示 
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            json jsonArray;
            for (const auto& byte : value) {
                jsonArray.push_back(byte); // 转换为 JSON 数组
            }
            return jsonArray;
        } else {
            return value; // 其他类型直接转换
        }
    }, fieldValue);
}

// 将 std::vector<std::variant<...>> 转换为 JSON 格式
json vectorToJson(const std::vector<FieldValue>& values) {
    json jsonArray = json::array();
    for (const auto& value : values) {
        jsonArray.push_back(FieldValueToJson(value));
    }
    return jsonArray;
}

std::ostream& operator<<(std::ostream& os, const FieldValue& fieldValue) {
    os << FieldValueToJson(fieldValue).dump(); // 使用 JSON 的 dump 方法输出为字符串
    return os;
};


FieldValue getDefault(const std::string& type) {
    /*
    if (type == "int") {
        return 0;
    } else if (type == "double") {
        return 0.0;
    } else if (type == "string") {
        return std::string{};
    } else if (type == "bool") {
        return false;
    } else if (type == "date") {
        return std::time(nullptr);
    } else if (type == "array") {
        return std::vector<uint8_t>{};
    }*/
    if (type == "date") {
        return std::time(nullptr);
    }
    return std::monostate{};
}


FieldValue jsonToFieldValue(const std::string& type,const json& value) {
    if (type == "int") {
		return value.get<int>();
	} else if (type == "double") {
		return value.get<double>();
	} else if (type == "string") {
		return value.get<std::string>();
	} else if (type == "bool") {
		return value.get<bool>();
	} else if (type == "date") {
		return stringToTimeT(value.get<std::string>());
	} else if (type == "array") {
		return std::vector<uint8_t>(value.begin(), value.end());
	}
    throw std::invalid_argument("Unknown type for default value");
};

std::vector<FieldValue> jsonToFieldValues(const std::vector<std::string>& types, const json& values) {
    // 检查类型和值的数量是否匹配
    if (types.size() != values.size()) {
        throw std::invalid_argument("Mismatch between types and values count");
    }

    std::vector<FieldValue> fieldValues;
    fieldValues.reserve(types.size());

    for (size_t i = 0; i < types.size(); ++i) {
        fieldValues.push_back(jsonToFieldValue(types[i], values[i]));
    }

    return fieldValues;
}




bool isValidType(const FieldValue& value, const std::string& type) {
    if (type == "array") {
        return std::holds_alternative<std::vector<uint8_t>>(value);
    }
    if (type == "date") {
        return std::holds_alternative<std::time_t>(value);
    }
    if (type == "int") {
        return std::holds_alternative<int>(value);
    }
    if (type == "bool") {
        return std::holds_alternative<bool>(value);
    }
    if (type == "double") {
        return std::holds_alternative<double>(value);
    }
    if (type == "string") {
        return std::holds_alternative<std::string>(value);
    }

    return false; // 默认返回 false
}
