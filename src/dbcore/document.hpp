#ifndef DOCUMENT_HPP
#define DOCUMENT_HPP

#include "field.hpp"

class Document {
public:
    // 添加或更新字段
    void setField(const std::string& fieldName, const std::shared_ptr<Field> field) {
        fields_[fieldName] = field;
    }

	std::unordered_map<std::string, std::shared_ptr<Field>> getFields() const{
		return fields_;
	}

	// 检查字段是否存在
    bool hasField(const std::string& fieldName) const {
        return fields_.find(fieldName) != fields_.end();
    }

    // 获取字段
    std::shared_ptr<Field> getField(const std::string& fieldName) const ;

	std::shared_ptr<Field> getFieldByPath(const std::string& path) const ;
	// 添加新字段!!!不能更新
	void setFieldByPath(const std::string& path, const Field& field);

	bool removeFieldByPath(const std::string& path);
    
	json toJson() const;
	std::string toBinary() const;
    void fromBinary(const char* data, size_t size);
    void fromJson(const json& j);

private:
	std::unordered_map<std::string, std::shared_ptr<Field>> fields_;
};
std::ostream& operator<<(std::ostream& os, const Document& doc);

#endif