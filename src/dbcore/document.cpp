#include "document.hpp"

std::ostream& operator<<(std::ostream& os, const Document& doc) {
	for (const auto& [name, field] : doc.getFields()) {
		os << "Field: " << name << " => " << field << "\n";
	}
	return os;
}

json Document::toJson() const {
	json j;
	for (const auto& field : fields_) {
		j[field.first] = field.second.toJson();
	}
	return j;
}

std::string Document::toBinary() const {
	std::ostringstream outStream(std::ios::binary);
	for (const auto& [name, field] : fields_) {
		size_t nameLength = name.size();
		outStream.write(reinterpret_cast<const char*>(&nameLength), sizeof(nameLength));
		outStream.write(name.data(), nameLength); // 写入字段名称

		std::string fieldBinaryData = field.toBinary();
		size_t fieldSize = fieldBinaryData.size();
		outStream.write(reinterpret_cast<const char*>(&fieldSize), sizeof(fieldSize));
		outStream.write(fieldBinaryData.data(), fieldSize);
	}
	return outStream.str();
}

void Document::fromBinary(const char* data, size_t size) {
	std::istringstream inStream(std::string(data, size), std::ios::binary);
	while (inStream.peek() != EOF) {
		size_t nameLength;
		inStream.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));

		std::string name(nameLength, '\0');
		inStream.read(&name[0], nameLength); // 读取字段名称

		size_t fieldSize;
		inStream.read(reinterpret_cast<char*>(&fieldSize), sizeof(fieldSize));

		std::string fieldData(fieldSize, '\0');
		inStream.read(&fieldData[0], fieldSize); // 读取字段数据

		Field field;
		field.fromBinary(fieldData.c_str(), fieldSize); // 使用 fromBinary 恢复字段数据

		fields_[name] = field; // 将恢复的字段添加到 fields_
	}
}

void Document::fromJson(const json& j) {
	for (const auto& [key, val] : j.items()) {
		// 获取字段名并根据字段名来处理
		auto& field = fields_[key];  // 假设 fields_ 是一个存储字段的 map
		field.fromJson(val);  // 调用 Field 的 fromJson 方法
	}
}