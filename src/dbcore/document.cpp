#include "document.hpp"

std::ostream& operator<<(std::ostream& os, const Document& doc) {
	for (const auto& [name, field] : doc.getFields()) {
		os << "Field: " << name << " => " << field << "\n";
	}
	return os;
}

json Document::toJson() const {
	json jsonFields;
	for (const auto& [key,field] : fields_) {
		//std::cout << "key: " << key << " v: " << *field << std::endl;
		jsonFields[key] = field.toJson();
	}
	return jsonFields;
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

    // 缓冲区用于存储字段名称和数据
    size_t nameLength, fieldSize;
    std::string name, fieldData;

    while (inStream.peek() != EOF) {
        // 读取字段名称的长度并直接在缓冲区中分配空间
        inStream.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));
        name.resize(nameLength);
        inStream.read(&name[0], nameLength); // 读取字段名称

        // 读取字段数据的长度并在缓冲区中分配空间
        inStream.read(reinterpret_cast<char*>(&fieldSize), sizeof(fieldSize));
        fieldData.resize(fieldSize);
        inStream.read(&fieldData[0], fieldSize); // 读取字段数据

        // 通过二进制数据恢复字段内容
        Field field;
        field.fromBinary(fieldData.c_str(), fieldSize);

        // 将字段添加到文档的 fields_ 中
        fields_[name] = std::move(field); // 使用 move 避免不必要的拷贝
    }
}

void Document::fromJson(const json& j) {
    for (const auto& [key, val] : j.items()) {
        // 如果字段已存在，跳过插入
        auto insert_result = fields_.insert({key, Field{}});
        if (!insert_result.second) {
            std::cerr << "Error processing key: " << key << " with value: " << val
                      << ". Error: Duplicate field key." << std::endl;
            throw std::invalid_argument("Duplicate field key: " + key);
        }

        // 对新插入的字段进行处理
        try {
            insert_result.first->second.fromJson(val);
        } catch (const std::exception& e) {
            std::cerr << "Error processing key: " << key << " with value: " << val
                      << ". Error: " << e.what() << std::endl;
            throw;
        }
    }
}

const Field* Document::getField(const std::string& fieldName) const {
	auto it = fields_.find(fieldName);
	if (it != fields_.end()) {
		return &(it->second);
	} else {
		return nullptr;
	}
}

Field* Document::getField(const std::string& fieldName) {
    auto it = fields_.find(fieldName);
    if (it != fields_.end()) {
        return &(it->second);
    }
    return nullptr;
}

void Document::setFieldByPath(const std::string& path, const Field& field) {
    size_t pos = path.find('.');

    if (pos == std::string::npos) {
        // 如果路径没有嵌套，直接在顶层设置字段
		fields_.emplace(path,field);
        //fields_[path] = std::make_shared<Field>(field);
    } else {
        // 如果路径有嵌套，解析嵌套字段
        std::string currentField = path.substr(0, pos);
        std::string remainingPath = path.substr(pos + 1);
//std::cout << "currentField: " << currentField << " remainingPath: " << remainingPath << std::endl;
        auto it = fields_.find(currentField);
        std::shared_ptr<Document> nestedDoc;

        if (it != fields_.end()) {
            // 如果字段已存在且是一个嵌套文档，获取该嵌套文档
            if (it->second.getType() == FieldType::DOCUMENT) {
                nestedDoc = std::get<std::shared_ptr<Document>>(it->second.getValue());
            } else {
                // 如果存在同名字段但不是文档，抛出异常或覆盖为文档
                throw std::runtime_error("Field '" + currentField + "' is not a document");
            }
        }

        if (!nestedDoc) {
            // 如果嵌套文档不存在，创建一个新的嵌套文档
            nestedDoc = std::make_shared<Document>();
            fields_[currentField] = Field(nestedDoc);
        }

        // 在嵌套文档中递归设置字段
        nestedDoc->setFieldByPath(remainingPath, field);
    }
}


Field* Document::getFieldByPath(const std::string& path) {
    size_t pos = path.find('.');
	//std::cout << "searching for path: " << path << "\n";
    if (pos == std::string::npos) {
        // 没有嵌套，直接返回顶层字段
        return getField(path);
    } else {
        // 解析嵌套字段
        std::string currentField = path.substr(0, pos);
		//std::cout << "finding " << currentField << "\n";
        auto it = fields_.find(currentField);
        if (it != fields_.end() && it->second.getType() == FieldType::DOCUMENT) {
            const auto& nestedDoc = std::get<std::shared_ptr<Document>>(it->second.getValue());
            if (nestedDoc) {
                return nestedDoc->getFieldByPath(path.substr(pos + 1));
            }
        }
    }
    return nullptr;
}

bool Document::removeFieldByPath(const std::string& path) {
    size_t pos = path.find('.');
    if (pos == std::string::npos) {
        // 删除顶层字段
        auto it = fields_.find(path);
        if (it != fields_.end()) {
            fields_.erase(it);
            return true; // 删除成功
        }
        return false; // 字段不存在
    } else {
        // 解析嵌套字段
        std::string currentField = path.substr(0, pos);
        auto it = fields_.find(currentField);
        if (it != fields_.end() && it->second.getType() == FieldType::DOCUMENT) {
            auto nestedDoc = std::get<std::shared_ptr<Document>>(it->second.getValue());
            if (nestedDoc) {
                return nestedDoc->removeFieldByPath(path.substr(pos + 1));
            }
        }
        return false; // 嵌套字段不存在
    }
}
