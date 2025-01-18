#include "memcolumn.hpp"
#include "util/util.hpp"

// Field 转 JSON 的函数
json fieldToJson(const Field& field) {
    return std::visit([](auto&& value) -> json {
        using T = std::decay_t<decltype(value)>;
        // 处理 std::monostate
        if constexpr (std::is_same_v<T, std::monostate>) {
            return nullptr;  // 返回 null，表示空值
        } else if constexpr (std::is_same_v<T, std::time_t>) {
			std::string timeStr = std::ctime(&value);
			timeStr.erase(std::remove(timeStr.begin(), timeStr.end(), '\n'), timeStr.end());
			return timeStr;
            //return std::ctime(&value);// 转换为时间戳表示 
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            json jsonArray;
            for (const auto& byte : value) {
                jsonArray.push_back(byte); // 转换为 JSON 数组
            }
            return jsonArray;
        } else {
            return value; // 其他类型直接转换
        }
    }, field);
}

std::ostream& operator<<(std::ostream& os, const Field& field) {
    os << fieldToJson(field).dump(); // 使用 JSON 的 dump 方法输出为字符串
    return os;
};


Field getDefault(const std::string& type) {
    /*
    if (type == "int") {
        return 0;
    } else if (type == "double") {
        return 0.0;
    } else if (type == "string") {
        return std::string{};
    } else if (type == "bool") {
        return false;
    } else if (type == "date") {
        return std::time(nullptr);
    } else if (type == "array") {
        return std::vector<uint8_t>{};
    }*/
    if (type == "date") {
        return std::time(nullptr);
    }
    return std::monostate{};
}


Field jsonToField(const std::string& type,const json& value) {
    if (type == "int") {
		return value.get<int>();
	} else if (type == "double") {
		return value.get<double>();
	} else if (type == "string") {
		return value.get<std::string>();
	} else if (type == "bool") {
		return value.get<bool>();
	} else if (type == "date") {
		return stringToTimeT(value.get<std::string>());
	} else if (type == "array") {
		return std::vector<uint8_t>(value.begin(), value.end());
	}
    throw std::invalid_argument("Unknown type for default value");
};



bool isValidType(const Field& value, const std::string& type) {
    if (type == "array") {
        return std::holds_alternative<std::vector<uint8_t>>(value);
    }
    if (type == "date") {
        return std::holds_alternative<std::time_t>(value);
    }
    if (type == "int") {
        return std::holds_alternative<int>(value);
    }
    if (type == "bool") {
        return std::holds_alternative<bool>(value);
    }
    if (type == "double") {
        return std::holds_alternative<double>(value);
    }
    if (type == "string") {
        return std::holds_alternative<std::string>(value);
    }

    return false; // 默认返回 false
}

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
            col.value("primaryKey", false)
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

        // 使用 fieldToJson 转换 defaultValue 为 JSON
        if (column.defaultValue.index() != std::variant_npos) {
            colJson["defaultValue"] = fieldToJson(column.defaultValue);
        }

        jsonColumns.push_back(colJson);
    }

    return jsonColumns;
}
