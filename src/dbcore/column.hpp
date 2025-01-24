#ifndef column_HPP
#define column_HPP

#include <iostream>
#include "fieldvalue.hpp"


struct Column {
    std::string name = "";
    std::string type = "";
    bool nullable = false;
    FieldValue defaultValue = std::monostate{};;
    bool primaryKey = false;
    bool indexed = false;
};

std::vector<Column> jsonToColumns(const json& jsonColumns);
json columnsToJson(const std::vector<Column>& columns);
FieldValue getDefault(const std::string& type);

#endif