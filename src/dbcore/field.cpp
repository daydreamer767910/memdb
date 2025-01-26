#include "field.hpp"
#include "document.hpp"

std::ostream& operator<<(std::ostream& os, const Field& field) {
	os << field.getValue();
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
        seed ^= Field::Hash{}(*field); // 对每个 Field 递归计算哈希
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

void Field::fromBinary(const char* data, size_t size) {
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
}

json Field::toJson() const {
    return valuetoJson(value_);
}

void Field::fromJson(const json& value) {
    value_ = valuefromJson(value);
}

bool Field::typeMatches(const FieldType& type) const{
	switch (type) {
        case FieldType::INT: return std::holds_alternative<int>(value_);
        case FieldType::DOUBLE: return std::holds_alternative<double>(value_);
        case FieldType::BOOL: return std::holds_alternative<bool>(value_);
        case FieldType::STRING: return std::holds_alternative<std::string>(value_);
        case FieldType::TIME: return std::holds_alternative<std::time_t>(value_);
        case FieldType::BINARY: return std::holds_alternative<std::vector<uint8_t>>(value_);
        case FieldType::DOCUMENT: return std::holds_alternative<std::shared_ptr<Document>>(value_);
        case FieldType::NONE: return std::holds_alternative<std::monostate>(value_);
        default: return false;
    }
}
