#ifndef column_HPP
#define column_HPP

#include <iostream>
#include "field.hpp"


struct Column {
    std::string name = "";
    std::string type = "";
    bool nullable = false;
    Field defaultValue = std::monostate{};;
    bool primaryKey = false;
    bool indexed = false;
};

using Document = std::unordered_map<std::string, Field>;
using Row = std::vector<Field>;
// Define an index type
using Index = std::map<Field, std::set<size_t>>;

// 定义主键索引
using PrimaryKeyIndex = std::unordered_map<Field, size_t, FieldHash, FieldEqual>;

std::vector<Column> jsonToColumns(const json& jsonColumns);
json columnsToJson(const std::vector<Column>& columns);

#endif