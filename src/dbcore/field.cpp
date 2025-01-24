#include "field.hpp"
#include "document.hpp"

std::ostream& operator<<(std::ostream& os, const Field& field) {
	try {
		if (std::holds_alternative<std::monostate>(field.getValue())) {
			os << "Uninitialized";
		} else if (std::holds_alternative<int>(field.getValue())) {
			os << field.getValue<int>();
		} else if (std::holds_alternative<double>(field.getValue())) {
			os << field.getValue<double>();
		} else if (std::holds_alternative<bool>(field.getValue())) {
			os << (field.getValue<bool>() ? "true" : "false");
		} else if (std::holds_alternative<std::string>(field.getValue())) {
			os << field.getValue<std::string>();
		} else if (std::holds_alternative<std::time_t>(field.getValue())) {
			std::time_t time = field.getValue<std::time_t>();
			std::string timeStr = std::ctime(&time);
			timeStr.erase(std::remove(timeStr.begin(), timeStr.end(), '\n'), timeStr.end());
			os << timeStr; // 格式化时间为可读字符串
		} else if (std::holds_alternative<std::vector<uint8_t>>(field.getValue())) {
			const auto& vec = field.getValue<std::vector<uint8_t>>();
			os << "[ ";
			for (uint8_t byte : vec) {
				os << static_cast<int>(byte) << " "; // 打印为整数
			}
			os << "]";
		} else if (std::holds_alternative<std::shared_ptr<Document>>(field.getValue())) {
			os << "Nested Document:\n";
			os << *field.getValue<std::shared_ptr<Document>>(); // 假定 Document 类也有 operator<<
		} else {
			os << "Unknown type";
		}
	} catch (const std::exception& e) {
		os << "Error: " << e.what();
	} catch (...) {
		os << "Unknown error in retrieving value";
	}

	return os;
}

std::size_t Field::hashDocumentPtr(const std::shared_ptr<Document>& doc) {
    if (!doc) {
        return 0; // 空指针返回固定哈希值
    }
    // 计算 Document 的哈希值（可以自定义逻辑）
    // 比如，对 Document 的某些字段计算哈希
    std::size_t seed = 0;
    for (const auto& [name, field] : doc->getFields()) {
        seed ^= std::hash<std::string>{}(name) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= Field::Hash{}(field); // 对每个 Field 递归计算哈希
    }
    return seed;
}

std::string Field::toBinary() const {
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
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Document>>) {
            // 序列化嵌套 Document
            std::string docBinary = value->toBinary();
            size_t length = docBinary.size();
            outStream.write(reinterpret_cast<const char*>(&length), sizeof(length));
            outStream.write(docBinary.data(), length);
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
		case 7: { // std::shared_ptr<Document>
            size_t length;
            inStream.read(reinterpret_cast<char*>(&length), sizeof(length));
            std::string docBinary(length, '\0');
            inStream.read(docBinary.data(), length);
            auto doc = std::make_shared<Document>();
            doc->fromBinary(docBinary.data(), docBinary.size());
            value_ = doc;
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
            return value; // 直接转换为 JSON 数组
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Document>>) {
            return value->toJson();  // 递归调用 Document 的 to_json
        } else {
            return value; // 其他类型直接转换
        }
    }, value_);
}

Field& Field::fromJson(const std::string& type, const json& value) {
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
    } else if (type == "document") {
        auto doc = std::make_shared<Document>();
        doc->fromJson(value);
        value_ = doc;
    } else {
        throw std::invalid_argument("Unknown type for default value");
    }
    return *this;
}

Field& Field::fromJson(const json& value) {
    // 根据 JSON 值的类型来自动推断和赋值
    if (value.is_null()) {
        value_ = std::monostate{};  // 处理空值
    } else if (value.is_boolean()) {
        value_ = value.get<bool>();
    } else if (value.is_number_integer()) {
        value_ = value.get<int>();
    } else if (value.is_number_float()) {
        value_ = value.get<double>();
    } else if (value.is_string()) {
        value_ = value.get<std::string>();
    } else if (value.is_array()) {
        value_ = std::vector<uint8_t>(value.begin(), value.end());
    } else if (value.is_object()) {
        auto doc = std::make_shared<Document>();
        doc->fromJson(value);  // 假设 Document 的 fromJson 处理的是对象
        value_ = doc;
    } else {
        throw std::invalid_argument("Unknown type for default value");
    }
    return *this;
}


