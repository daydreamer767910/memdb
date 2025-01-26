#ifndef COLLECTION_SCHEMA_HPP
#define COLLECTION_SCHEMA_HPP

#include "schema.hpp"
#include "field_schema.hpp"
#include "document.hpp"

class CollectionSchema : public Schema {
public:
    // 默认构造函数
    CollectionSchema() = default;

    // 从 JSON 配置构建 CollectionSchema
    void fromJson(const json& j) override {
        if (!j.contains("fields")) {
            throw std::invalid_argument("Invalid JSON: Missing 'fields' key");
        }

        // Parse fields
        const auto& fields = j.at("fields");
        for (auto it = fields.begin(); it != fields.end(); ++it) {
            const std::string& fieldName = it.key();
            FieldSchema fieldSchema;
            fieldSchema.fromJson(it.value());
            fieldSchemas_.emplace(fieldName, fieldSchema);
        }
    }

    // 转换为 JSON 格式
    json toJson() const {
        json j;
        for (const auto& [fieldName, fieldSchema] : fieldSchemas_) {
            j["fields"][fieldName] = fieldSchema.toJson();
        }
        return j;
    }

    // 获取默认配置
    json getDefault() const override {
        json defaultJson;

        for (const auto& [fieldName, fieldSchema] : fieldSchemas_) {
            const auto& constraints = fieldSchema.getConstraints();
            if (constraints.defaultValue.has_value()) {
                // Populate the field with the default value
                const auto& defaultValue = constraints.defaultValue.value();
                if (std::holds_alternative<int>(defaultValue)) {
                    defaultJson[fieldName] = std::get<int>(defaultValue);
                } else if (std::holds_alternative<double>(defaultValue)) {
                    defaultJson[fieldName] = std::get<double>(defaultValue);
                } else if (std::holds_alternative<bool>(defaultValue)) {
                    defaultJson[fieldName] = std::get<bool>(defaultValue);
                } else if (std::holds_alternative<std::string>(defaultValue)) {
                    defaultJson[fieldName] = std::get<std::string>(defaultValue);
                } else if (std::holds_alternative<std::monostate>(defaultValue)) {
                    defaultJson[fieldName] = nullptr;  // Explicitly set to null
                }
                // Add more cases for other FieldValue types as needed
            } else if (constraints.required) {
                // For required fields without default values, use a placeholder
                defaultJson[fieldName] = nullptr;
            }
        }

        return defaultJson;
    }

    // 验证整个文档是否符合 schema
    void validateDocument(const std::shared_ptr<Document> document) const;

private:
    // 存储字段名称及其对应的 schema
    std::unordered_map<std::string, FieldSchema> fieldSchemas_;
};

#endif // COLLECTION_SCHEMA_HPP
