#ifndef SCHEMA_HPP
#define SCHEMA_HPP

#include <iostream>
#include <string>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <map>
#include <memory>

// 使用 nlohmann::json 类型
using json = nlohmann::json;

class Schema {
public:
    virtual ~Schema() = default;

    // 从 JSON 配置构建 Schema
    virtual void fromJson(const json& j) = 0;

    // 转换为 JSON 格式
    virtual json toJson() const = 0;

    // 获取默认配置
    virtual json getDefault() const = 0;
};

#endif // SCHEMA_HPP
