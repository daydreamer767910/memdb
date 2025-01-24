#include "../registry.hpp"

class DeleteTableHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
        std::string tableName = task["name"];
		std::vector<std::string> conditions = task["conditions"].get<std::vector<std::string>>();
		std::vector<std::string> operators = task["ops"].get<std::vector<std::string>>();
		auto tb = db->getTable(tableName);
		std::vector<std::string> qtypes = tb->getColumnTypes(conditions);
		if (qtypes.size() != task["qvalues"].size()) {
			throw std::invalid_argument("Mismatch between types and values count");
		}
		std::vector<FieldValue> queryValues;
		queryValues.reserve(qtypes.size());
		for (size_t i = 0; i < qtypes.size(); ++i) {
			Field field;
			field.fromJson(qtypes[i],task["qvalues"][i]);
			queryValues.push_back(field.getValue());
		}
		size_t removed = tb->remove(conditions,queryValues,operators);
		response["response"] = "delete table sucess";
		response["status"] = "200";
		response["deleted"] = removed;
    }
};

REGISTER_ACTION("delete", DeleteTableHandler)