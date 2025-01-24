#ifndef COLLECTION_SCHEMA_HPP
#define COLLECTION_SCHEMA_HPP

#include "schema.hpp"

class CollectionSchema : public Schema {
public:
    // 代表 Collection 配置的字段
    std::string collectionName;
    std::vector<std::string> fields;
    std::map<std::string, std::string> additionalSettings;

    // 默认构造函数
    CollectionSchema() = default;

    // 从 JSON 配置构建 CollectionSchema
    void fromJson(const json& j) override {
        if (j.contains("name")) {
            collectionName = j["name"];
        } else {
            throw std::invalid_argument("Collection schema must contain a 'name' field.");
        }

        if (j.contains("fields")) {
            fields = j["fields"].get<std::vector<std::string>>();
        }

        if (j.contains("additionalSettings")) {
            additionalSettings = j["additionalSettings"].get<std::map<std::string, std::string>>();
        }
    }

    // 转换为 JSON 格式
    json toJson() const override {
        json j;
        j["name"] = collectionName;
        j["fields"] = fields;
        j["additionalSettings"] = additionalSettings;
        return j;
    }

    // 获取默认配置
    json getDefault() const override {
        json j;
        j["name"] = "defaultCollection";
        j["fields"] = {"field1", "field2"};
        j["additionalSettings"] = {};
        return j;
    }
};

#endif // COLLECTION_SCHEMA_HPP
