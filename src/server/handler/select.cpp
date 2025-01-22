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
		std::vector<Field> queryValues = jsonToFields(qtypes,task["qvalues"]);
		if (tb) {
			auto ret = tb->query(columnNames,conditions,queryValues,operators,limit);
			response[tableName] = tb->rowsToJson(ret);
		} else {
			response["error"] = "table[" + tableName + "] not exist";
		}
    }
};

REGISTER_ACTION("select", SelectTableHandler)