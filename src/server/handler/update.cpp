#include "../registry.hpp"

class UpdateTableHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
        std::string tableName = task["name"];
		std::vector<std::string> columnNames = task["columns"].get<std::vector<std::string>>();
		std::vector<std::string> conditions = task["conditions"].get<std::vector<std::string>>();
		std::vector<std::string> operators = task["ops"].get<std::vector<std::string>>();

		auto tb = db->getTable(tableName);
		std::vector<std::string> vtypes = tb->getColumnTypes(columnNames);
		std::vector<std::string> qtypes = tb->getColumnTypes(conditions);
		std::vector<Field> newValues = jsonToFields(vtypes,task["values"]); // 新值
		std::vector<Field> queryValues = jsonToFields(qtypes,task["qvalues"]);

		if (tb) {
			size_t updated = tb->update(columnNames, newValues, conditions, queryValues, operators);
			if (updated>0) {
				response["response"] = "update table sucess";
				response["status"] = "200";
				response["updated"] = updated;
			} else {
				response["response"] = "update table fail";
				response["status"] = "300";
			}
		} else {
			response["error"] = "table[" + tableName + "] not exist";
		}
    }
};

REGISTER_ACTION("update", UpdateTableHandler)