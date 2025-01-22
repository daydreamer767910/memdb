#include "../registry.hpp"

class DeleteTableHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
        std::string tableName = task["name"];
		std::vector<std::string> conditions = task["conditions"].get<std::vector<std::string>>();
		std::vector<std::string> operators = task["ops"].get<std::vector<std::string>>();
		auto tb = db->getTable(tableName);
		std::vector<std::string> qtypes = tb->getColumnTypes(conditions);
		std::vector<Field> queryValues = jsonToFields(qtypes,task["qvalues"]);
		if (tb) {
			size_t removed = tb->remove(conditions,queryValues,operators);
			if (removed>0) {
				response["response"] = "delete table sucess";
				response["status"] = "200";
				response["deleted"] = removed;
			} else {
				response["response"] = "delete table fail";
				response["status"] = "300";
			}
		} else {
			response["error"] = "table[" + tableName + "] not exist";
		}
    }
};

REGISTER_ACTION("delete", DeleteTableHandler)