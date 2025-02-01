#ifndef DOCUMENT_HPP
#define DOCUMENT_HPP
#include <unordered_set>
#include "field.hpp"

using DocumentId = std::string;
using DocumentFields = std::unordered_map<std::string, Field>;

class Document {
public:
    // 添加或更新字段
    void setField(const std::string& fieldName, const Field& field) {
        fields_[fieldName] = field;
    }

    void setField(const std::string& fieldName, Field&& field) {
        fields_[fieldName] = std::move(field); // 直接移动 Field
    }

	const DocumentFields& getFields() const{
		return fields_;
	}

	// 检查字段是否存在
    bool hasField(const std::string& fieldName) const {
        return fields_.find(fieldName) != fields_.end();
    }

    // 获取字段
    const Field* getField(const std::string& fieldName) const ;
	
	Field* getFieldByPath(const std::string& path) ;
	// 添加新字段!!!不能更新
	void setFieldByPath(const std::string& path, const Field& field);

	bool removeFieldByPath(const std::string& path);
    
	virtual json toJson() const;
	std::string toBinary() const;
    void fromBinary(const char* data, size_t size);
    void fromJson(const json& j);

private:
	Field* getField(const std::string& fieldName);
	DocumentFields fields_;
};

std::ostream& operator<<(std::ostream& os, const Document& doc);

class ProjectionDocument:public Document {
public:
    ProjectionDocument(const std::shared_ptr<Document>& sourceDoc,
                       const std::unordered_set<std::string>& fieldsToProject);

    virtual json toJson() const;
private:
    // 使用 std::weak_ptr 来避免循环引用
    std::unordered_map<std::string, std::shared_ptr<const Field>> fields_;
};

#endif