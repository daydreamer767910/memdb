#include "../registry.hpp"

class SelectTableHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
        std::string tableName = task["name"];
		uint32_t limit = task["limit"];
		std::vector<std::string> columnNames = task["columns"].get<std::vector<std::string>>();
		std::vector<std::string> conditions = task["conditions"].get<std::vector<std::string>>();
		std::vector<std::string> operators = task["ops"].get<std::vector<std::string>>();
		auto tb = db->getTable(tableName);
		std::vector<std::string> qtypes = tb->getColumnTypes(conditions);
		std::vector<FieldValue> queryValues = jsonToFieldValues(qtypes,task["qvalues"]);
		auto ret = tb->query(columnNames,conditions,queryValues,operators,limit);
		for (auto& fields : ret) { //每一行数据
			json rowJson;
			for (size_t i = 0; i < columnNames.size(); ++i) { //每一列
				if (rowJson[columnNames[i]].is_null()) {
					rowJson[columnNames[i]] = json::array();
				}
				rowJson[columnNames[i]] = FieldValueToJson(fields[i]);
			}
			response["results"].push_back(rowJson);
		}
		// 总行数
		response["total"] = ret.size();
    }
};

REGISTER_ACTION("select", SelectTableHandler)