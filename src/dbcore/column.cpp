#include "column.hpp"
#include "util/util.hpp"
#include "field.hpp"



std::vector<Column> jsonToColumns(const json& jsonColumns) {
    //std::cout << jsonColumns.dump(4) << std::endl;
    std::vector<Column> columns = {};
    for (const auto& col : jsonColumns["columns"]) {
        FieldValue defaultValue;
        FieldType type = typefromJson(col);
        //std::cout << "type: " << type << std::endl;
        if (col.contains("defaultValue") && !col["defaultValue"].is_null()) {
            defaultValue = valuefromJson(col["defaultValue"]);
            //std::cout << "defaultValue: " << defaultValue << std::endl;
        } else {
            defaultValue = getDefault(type); // 使用 DefaultValue 类生成默认值
        }
        //if (!typeMatches(defaultValue, type)) {
        //    throw std::invalid_argument("type and default miss matched: " + typetoString(type));
        //}
        /*std::cout << "Processing column: " << col["name"] 
            << ", type: " << col["type"] 
            << ", default: " << defaultValue
            << std::endl;*/
        columns.push_back({
            col["name"],
            type,
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
        json colJson = typetoJson(column.type);;
        colJson["name"] = column.name;
        colJson["nullable"] = column.nullable;
        colJson["primaryKey"] = column.primaryKey;
        colJson["indexed"] = column.indexed;
        // 使用 FieldValueToJson 转换 defaultValue 为 JSON
        if (column.defaultValue.index() != std::variant_npos) {
            colJson["defaultValue"] = valuetoJson(column.defaultValue);
        }

        jsonColumns.push_back(colJson);
    }

    return jsonColumns;
}
