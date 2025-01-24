#ifndef DOCUMENT_HPP
#define DOCUMENT_HPP

#include "field.hpp"

class Document {
public:
    // 添加或更新字段
    void setField(const std::string& fieldName, const Field& field) {
        fields_[fieldName] = field;
    }

	std::unordered_map<std::string, Field> getFields() const{
		return fields_;
	}

	// 检查字段是否存在
    bool hasField(const std::string& fieldName) const {
        return fields_.find(fieldName) != fields_.end();
    }

    // 获取字段
    Field getField(const std::string& fieldName) const {
        auto it = fields_.find(fieldName);
        if (it != fields_.end()) {
            return it->second;
        } else {
            throw std::invalid_argument("Field not found: " + fieldName);
        }
    }

    
	json toJson() const;
	std::string toBinary() const;
    void fromBinary(const char* data, size_t size);
    void fromJson(const json& j);

private:
	std::unordered_map<std::string, Field> fields_;
};
std::ostream& operator<<(std::ostream& os, const Document& doc);

#endif