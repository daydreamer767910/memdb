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

		if (vtypes.size() != task["values"].size()) {
			throw std::invalid_argument("Mismatch between types and values count");
		}
		if (qtypes.size() != task["qvalues"].size()) {
			throw std::invalid_argument("Mismatch between types and values count");
		}


		std::vector<FieldValue> newValues;
		newValues.reserve(vtypes.size());
		for (size_t i = 0; i < vtypes.size(); ++i) {
			Field field;
			field.fromJson(vtypes[i],task["values"][i]);
			newValues.push_back(field.getValue());
		}

		std::vector<FieldValue> queryValues;
		queryValues.reserve(qtypes.size());
		for (size_t i = 0; i < qtypes.size(); ++i) {
			Field field;
			field.fromJson(qtypes[i],task["qvalues"][i]);
			queryValues.push_back(field.getValue());
		}

		size_t updated = tb->update(columnNames, newValues, conditions, queryValues, operators);
		response["response"] = "update table sucess";
		response["status"] = "200";
		response["updated"] = updated;
    }
};

REGISTER_ACTION("update", UpdateTableHandler)