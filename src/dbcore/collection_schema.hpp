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
        // Parse fields
        const auto& fields = j.at("schema");
        for (auto it = fields.begin(); it != fields.end(); ++it) {
            const std::string& fieldName = it.key();
            FieldSchema fieldSchema;
            fieldSchema.fromJson(it.value());
            fieldSchemas_.emplace(fieldName, fieldSchema);
        }
    }

    // 转换为 JSON 格式
    json toJson() const {
        if (fieldSchemas_.empty()) {
            return getDefault();
        }
        json j;
        for (const auto& [fieldName, fieldSchema] : fieldSchemas_) {
            j["schema"][fieldName] = fieldSchema.toJson();
        }
        return j;
    }

    // 获取默认配置
    json getDefault() const override {
        return json{
            {"schema", {
                {"_id", {{"type","string"}}}
            }}
        };
    }

    // 验证整个文档是否符合 schema
    void validateDocument(std::shared_ptr<const Document> document, uint8_t depth = 0) const;
    void validateField(const std::string& path, const Field& field) const;
private:
    // 存储字段名称及其对应的 schema
    std::unordered_map<std::string, FieldSchema> fieldSchemas_;
};

#endif // COLLECTION_SCHEMA_HPP
