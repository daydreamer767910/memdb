#include "field.hpp"

std::string Field::toBinary() const{
    std::ostringstream outStream(std::ios::binary);
    uint8_t typeIndex = value_.index(); // 获取当前类型的索引
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
    }, value_);

    return outStream.str();
}

Field& Field::fromBinary(const char* data, size_t size) {
    std::istringstream inStream(std::string(data, size), std::ios::binary);
    uint8_t typeIndex;
    inStream.read(reinterpret_cast<char*>(&typeIndex), sizeof(typeIndex));
    switch (typeIndex) {
        case 0: // std::monostate
            value_ = std::monostate{};
			break;
        case 1: { // int
            int value;
            inStream.read(reinterpret_cast<char*>(&value), sizeof(value));
            value_ = value;
			break;
        }
        case 2: { // double
            double value;
            inStream.read(reinterpret_cast<char*>(&value), sizeof(value));
            value_ = value;
			break;
        }
        case 3: { // bool
            uint8_t boolValue;
            inStream.read(reinterpret_cast<char*>(&boolValue), sizeof(boolValue));
            value_ = boolValue != 0;
			break;
        }
        case 4: { // std::string
            size_t length;
            inStream.read(reinterpret_cast<char*>(&length), sizeof(length));
            std::string value(length, '\0');
            inStream.read(value.data(), length);
            value_ = value;
			break;
        }
        case 5: { // std::time_t
            std::time_t value;
            inStream.read(reinterpret_cast<char*>(&value), sizeof(value));
            value_ = value;
			break;
        }
        case 6: { // std::vector<uint8_t>
            size_t length;
            inStream.read(reinterpret_cast<char*>(&length), sizeof(length));
            std::vector<uint8_t> value(length);
            inStream.read(reinterpret_cast<char*>(value.data()), length);
            value_ = value;
			break;
        }
        default:
            throw std::runtime_error("Unknown type index in binary data");
    }
	return *this;
}

json Field::toJson() const {
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
        //} else if constexpr (std::is_same_v<std::decay_t<decltype(val)>, std::shared_ptr<Document>>) {
                //return value->toJson();  // 递归调用 Document 的 to_json
        } else {
            return value; // 其他类型直接转换
        }
    }, value_);
}

Field& Field::fromJson(const std::string& type,const json& value) {
    if (type == "int") {
		value_ = value.get<int>();
	} else if (type == "double") {
		value_ = value.get<double>();
	} else if (type == "string") {
		value_ = value.get<std::string>();
	} else if (type == "bool") {
		value_ = value.get<bool>();
	} else if (type == "date") {
		value_ = stringToTimeT(value.get<std::string>());
	} else if (type == "array") {
		value_ = std::vector<uint8_t>(value.begin(), value.end());
	} else {
		throw std::invalid_argument("Unknown type for default value");
	}
    return *this;
};


