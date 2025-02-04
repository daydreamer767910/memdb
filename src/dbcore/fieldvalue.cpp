#include "fieldvalue.hpp"
#include "document.hpp"
// 定义输出运算符
std::ostream& operator<<(std::ostream& os, const FieldType& type) {
    static const char* type_strings[] = { "none", "int", "double", "bool", "string", "time", "binary", "document" };
    os << type_strings[static_cast<int>(type)];
    return os;
}

std::ostream& operator<<(std::ostream& os, const FieldValue& value) {
	try {
		if (std::holds_alternative<std::monostate>(value)) {
			os << "Uninitialized";
		} else if (std::holds_alternative<int>(value)) {
			os << std::get<int>(value);
		} else if (std::holds_alternative<double>(value)) {
			os << std::get<double>(value);
		} else if (std::holds_alternative<bool>(value)) {
			os << (std::get<bool>(value) ? "true" : "false");
		} else if (std::holds_alternative<std::string>(value)) {
			os << std::get<std::string>(value);
		} else if (std::holds_alternative<std::time_t>(value)) {
			std::time_t time = std::get<std::time_t>(value);
			std::string timeStr = std::ctime(&time);
			timeStr.erase(std::remove(timeStr.begin(), timeStr.end(), '\n'), timeStr.end());
			os << timeStr; // 格式化时间为可读字符串
		} else if (std::holds_alternative<std::vector<uint8_t>>(value)) {
			const auto& vec = std::get<std::vector<uint8_t>>(value);
			os << "[ ";
			for (uint8_t byte : vec) {
				os << static_cast<int>(byte) << " "; // 打印为整数
			}
			os << "]";
		} else if (std::holds_alternative<std::shared_ptr<Document>>(value)) {
			os << "Nested Document:\n";
			os << *std::get<std::shared_ptr<Document>>(value); // 假定 Document 类也有 operator<<
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

FieldType typefromJson(const json& j) {
	// Parse field type
	std::string typeStr = j.at("type").get<std::string>();
	if (typeStr == "int") return FieldType::INT;
	else if (typeStr == "string") return FieldType::STRING;
	else if (typeStr == "double") return FieldType::DOUBLE;
	else if (typeStr == "bool") return FieldType::BOOL;
	else if (typeStr == "time") return FieldType::TIME;
	else if (typeStr == "binary") return FieldType::BINARY;
	else if (typeStr == "document") return FieldType::DOCUMENT;
	else return FieldType::NONE;
}

json typetoJson(const FieldType& type) {
    json j;
    switch (type) {
        case FieldType::INT: j["type"] = "int"; break;
        case FieldType::DOUBLE: j["type"] = "double"; break;
        case FieldType::BOOL: j["type"] = "bool"; break;
        case FieldType::STRING: j["type"] = "string"; break;
        case FieldType::TIME: j["type"] = "time"; break;
        case FieldType::BINARY: j["type"] = "binary"; break;
        case FieldType::DOCUMENT: j["type"] = "document"; break;
        default: j["type"] = "none"; break;
    }
    return j;
}

std::string typetoString(const FieldType& type) {
	std::string s;
    switch (type) {
        case FieldType::INT: s = "int"; break;
        case FieldType::DOUBLE: s = "double"; break;
        case FieldType::BOOL: s = "bool"; break;
        case FieldType::STRING: s = "string"; break;
        case FieldType::TIME: s = "time"; break;
        case FieldType::BINARY: s = "binary"; break;
        case FieldType::DOCUMENT: s = "document"; break;
        default: s = "none"; break;
    }
    return s;
}

FieldType typefromString(const std::string& typeStr) {
	if (typeStr == "int") return FieldType::INT;
	else if (typeStr == "string") return FieldType::STRING;
	else if (typeStr == "double") return FieldType::DOUBLE;
	else if (typeStr == "bool") return FieldType::BOOL;
	else if (typeStr == "time") return FieldType::TIME;
	else if (typeStr == "binary") return FieldType::BINARY;
	else if (typeStr == "document") return FieldType::DOCUMENT;
	else return FieldType::NONE;
}

FieldValue getDefault(const FieldType& type) {
    if (type == FieldType::TIME) {
        return std::time(nullptr);
    }
    return std::monostate{};
}


FieldValue valuefromJson(const json& value) {
    // 根据 JSON 值的类型来自动推断和赋值
    if (value.is_null()) {
        return std::monostate{};  // 处理空值
    } else if (value.is_boolean()) {
        return value.get<bool>();
    } else if (value.is_number_integer()) {
        return value.get<int>();
    } else if (value.is_number_float()) {
        return value.get<double>();
    } else if (value.is_string()) {
		if (isDate(value.get<std::string>())) {
			return stringToTimeT(value.get<std::string>());
		}
        return value.get<std::string>();
    } else if (value.is_array()) {
        // 验证是否是字节数组
        if (!value.is_array() || !std::all_of(value.begin(), value.end(), 
				[](const json& elem) { 
					return elem.is_number_unsigned();
				})) {
            throw std::invalid_argument("Invalid binary type for FieldValue");
        }
        return std::vector<uint8_t>(value.begin(), value.end());
    } else if (value.is_object()) {
        auto doc = std::make_shared<Document>();
        doc->fromJson(value);  // 假设 Document 的 fromJson 处理的是对象
        return doc;
    } else {
        throw std::invalid_argument("Unknown type for value");
    }
}

json valuetoJson(const FieldValue& value) {
    return std::visit([](auto&& v) -> json {
        using T = std::decay_t<decltype(v)>;
        // 处理 std::monostate
        if constexpr (std::is_same_v<T, std::monostate>) {
            return nullptr;  // 返回 null，表示空值
        } else if constexpr (std::is_same_v<T, std::time_t>) {
			std::string timeStr = std::ctime(&v);
			timeStr.erase(std::remove(timeStr.begin(), timeStr.end(), '\n'), timeStr.end());
			return timeStr;
            //return std::ctime(&value);// 转换为时间戳表示 
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            return v; // 直接转换为 JSON 数组
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Document>>) {
			return v->toJson();  // 调用 Document 的 toJson 方法
        } else {
            return v; // 其他类型直接转换
        }
    }, value);
}

// 对 FieldValue 类型重载比较操作符
bool operator<(const FieldValue& lhs, const FieldValue& rhs) {
    return std::visit([](const auto& l, const auto& r) -> bool {
        using T1 = std::decay_t<decltype(l)>;
        using T2 = std::decay_t<decltype(r)>;
        if constexpr (std::is_same_v<T1, T2>) {
            return l < r; // 如果类型相同，直接比较
        } else {
            return false; // 不同类型返回 false
        }
    }, lhs, rhs);
}

bool operator==(const FieldValue& lhs, const FieldValue& rhs) {
    return std::visit([](const auto& l, const auto& r) -> bool {
        using T1 = std::decay_t<decltype(l)>;
        using T2 = std::decay_t<decltype(r)>;
        if constexpr (std::is_same_v<T1, T2>) {
            return l == r; // 如果类型相同，直接比较
        } else {
            return false; // 不同类型返回 false
        }
    }, lhs, rhs);
}
