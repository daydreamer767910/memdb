#ifndef DOCUMENT_HPP
#define DOCUMENT_HPP

#include "field.hpp"

class Document {
public:
    std::unordered_map<std::string, Field> fields_;

    // 添加或更新字段
    void setField(const std::string& fieldName, const Field& field) {
        fields_[fieldName] = field;
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

    // 检查字段是否存在
    bool hasField(const std::string& fieldName) const {
        return fields_.find(fieldName) != fields_.end();
    }

	json toJson() const {
        json j;
        for (const auto& field : fields_) {
            j[field.first] = field.second.to_json();
        }
        return j;
    }

    // 输出文档内容
    void print() const {
        for (const auto& [name, field] : fields_) {
            std::cout << "Field: " << name << " => ";
            try {
                if (std::holds_alternative<int>(field.value_)) {
                    std::cout << field.getValue<int>() << std::endl;
                } else if (std::holds_alternative<double>(field.value_)) {
                    std::cout << field.getValue<double>() << std::endl;
                } else if (std::holds_alternative<bool>(field.value_)) {
                    std::cout << field.getValue<bool>() << std::endl;
                } else if (std::holds_alternative<std::string>(field.value_)) {
                    std::cout << field.getValue<std::string>() << std::endl;
                } else if (std::holds_alternative<std::shared_ptr<Document>>(field.value_)) {
                    std::cout << "Nested Document" << std::endl;
                    field.getValue<std::shared_ptr<Document>>()->print();  // 递归打印嵌套文档
                }
            } catch (...) {
                std::cout << "Error in retrieving value" << std::endl;
            }
        }
    }
};

#endif