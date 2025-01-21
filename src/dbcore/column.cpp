#include "column.hpp"
#include "util/util.hpp"

std::vector<Column> jsonToColumns(const json& jsonColumns) {
    //std::cout << jsonColumns.dump(4) << std::endl;
    std::vector<Column> columns = {};
    for (const auto& col : jsonColumns["columns"]) {
        Field defaultValue;
        if (col.contains("defaultValue") && !col["defaultValue"].is_null()) {
            const auto& defaultVal = col["defaultValue"];
            if (col["type"] == "int") {
                defaultValue = defaultVal.get<int>();
            } else if (col["type"] == "double") {
                defaultValue = defaultVal.get<double>();
            } else if (col["type"] == "string") {
                defaultValue = defaultVal.get<std::string>();
            } else if (col["type"] == "bool") {
                defaultValue = defaultVal.get<bool>();
            } else if (col["type"] == "date") {
                //defaultValue = defaultVal.get<std::time_t>();
                defaultValue = stringToTimeT(defaultVal.get<std::string>());
            } else if (col["type"] == "array") {
                defaultValue = std::vector<uint8_t>(defaultVal.begin(), defaultVal.end());
            }
        } else {
            defaultValue = getDefault(col["type"]); // 使用 DefaultValue 类生成默认值
        }
        /*std::cout << "Processing column: " << col["name"] 
            << ", type: " << col["type"] 
            << ", default: " << defaultValue
            << std::endl;*/
        columns.push_back({
            col["name"],
            col["type"],
            col.value("nullable", false),
            defaultValue,
            col.value("primaryKey", false),
            col.value("indexed", false)
        });
    }
    return columns;
}

json columnsToJson(const std::vector<Column>& columns) {
    json jsonColumns = json::array();
    for (const auto& column : columns) {
        json colJson;
        colJson["name"] = column.name;
        colJson["type"] = column.type;
        colJson["nullable"] = column.nullable;
        colJson["primaryKey"] = column.primaryKey;
        colJson["indexed"] = column.indexed;
        // 使用 fieldToJson 转换 defaultValue 为 JSON
        if (column.defaultValue.index() != std::variant_npos) {
            colJson["defaultValue"] = fieldToJson(column.defaultValue);
        }

        jsonColumns.push_back(colJson);
    }

    return jsonColumns;
}
