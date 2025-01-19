#include "field.hpp"

std::string FieldToBinary(const Field& field) {
    std::ostringstream outStream(std::ios::binary);
    uint8_t typeIndex = field.index(); // 获取当前类型的索引
    outStream.write(reinterpret_cast<const char*>(&typeIndex), sizeof(typeIndex));

    std::visit([&outStream](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            // 不需要写入任何内容
        } else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, double> || std::is_same_v<T, std::time_t>) {
            outStream.write(reinterpret_cast<const char*>(&value), sizeof(value));
        } else if constexpr (std::is_same_v<T, bool>) {
            uint8_t boolValue = value ? 1 : 0;
            outStream.write(reinterpret_cast<const char*>(&boolValue), sizeof(boolValue));
        } else if constexpr (std::is_same_v<T, std::string>) {
            size_t length = value.size();
            outStream.write(reinterpret_cast<const char*>(&length), sizeof(length));
            outStream.write(value.data(), length);
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            size_t length = value.size();
            outStream.write(reinterpret_cast<const char*>(&length), sizeof(length));
            outStream.write(reinterpret_cast<const char*>(value.data()), length);
        }
    }, field);

    return outStream.str();
}

Field FieldFromBinary(const char* data, size_t size) {
    std::istringstream inStream(std::string(data, size), std::ios::binary);

    uint8_t typeIndex;
    inStream.read(reinterpret_cast<char*>(&typeIndex), sizeof(typeIndex));

    switch (typeIndex) {
        case 0: // std::monostate
            return std::monostate{};
        case 1: { // int
            int value;
            inStream.read(reinterpret_cast<char*>(&value), sizeof(value));
            return value;
        }
        case 2: { // double
            double value;
            inStream.read(reinterpret_cast<char*>(&value), sizeof(value));
            return value;
        }
        case 3: { // bool
            uint8_t boolValue;
            inStream.read(reinterpret_cast<char*>(&boolValue), sizeof(boolValue));
            return boolValue != 0;
        }
        case 4: { // std::string
            size_t length;
            inStream.read(reinterpret_cast<char*>(&length), sizeof(length));
            std::string value(length, '\0');
            inStream.read(value.data(), length);
            return value;
        }
        case 5: { // std::time_t
            std::time_t value;
            inStream.read(reinterpret_cast<char*>(&value), sizeof(value));
            return value;
        }
        case 6: { // std::vector<uint8_t>
            size_t length;
            inStream.read(reinterpret_cast<char*>(&length), sizeof(length));
            std::vector<uint8_t> value(length);
            inStream.read(reinterpret_cast<char*>(value.data()), length);
            return value;
        }
        default:
            throw std::runtime_error("Unknown type index in binary data");
    }
}

// Field 转 JSON 的函数
json fieldToJson(const Field& field) {
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
    }, field);
}

std::ostream& operator<<(std::ostream& os, const Field& field) {
    os << fieldToJson(field).dump(); // 使用 JSON 的 dump 方法输出为字符串
    return os;
};


Field getDefault(const std::string& type) {
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


Field jsonToField(const std::string& type,const json& value) {
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



bool isValidType(const Field& value, const std::string& type) {
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
