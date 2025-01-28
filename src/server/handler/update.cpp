#include "../registry.hpp"

class UpdateTableHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
		std::string name = task["name"];
		auto container = db->getContainer(name);
		// 准备响应
        response["response"] = "update container success";
        response["status"] = "200";
        
        if (container == nullptr) {
            response["response"] = "Container not found";
            response["status"] = "404";
            return;
        }
		try {
            if (container->getType() == "table") {
				std::vector<std::string> columnNames = task["columns"].get<std::vector<std::string>>();
				std::vector<std::string> conditions = task["conditions"].get<std::vector<std::string>>();
				std::vector<std::string> operators = task["ops"].get<std::vector<std::string>>();
				auto tb = std::dynamic_pointer_cast<Table>(container);
				if (!tb) {
                    throw std::runtime_error("Failed to cast to Table");
                }
				std::vector<FieldType> vtypes = tb->getColumnTypes(columnNames);
				std::vector<FieldType> qtypes = tb->getColumnTypes(conditions);

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
					field.fromJson(task["values"][i]);
					if (!field.typeMatches(vtypes[i])) {
						throw std::invalid_argument("Mismatch type between vtypes and values");
					}
					newValues.push_back(field.getValue());
				}

				std::vector<FieldValue> queryValues;
				queryValues.reserve(qtypes.size());
				for (size_t i = 0; i < qtypes.size(); ++i) {
					Field field;
					field.fromJson(task["qvalues"][i]);
					if (!field.typeMatches(qtypes[i])) {
						throw std::invalid_argument("Mismatch type between qtypes and qvalues");
					}
					queryValues.push_back(field.getValue());
				}

				size_t updated = tb->update(columnNames, newValues, conditions, queryValues, operators);
				response["updated"] = updated;
            } else if (container->getType() == "collection") {
                auto collection = std::dynamic_pointer_cast<Collection>(container);
                if (!collection) {
                    throw std::runtime_error("Failed to cast to Collection");
                }
                auto result = collection->updateFromJson(task);
                
                response["updated"] = result;
            } else {
                throw std::runtime_error("Unknown container type: " + container->getType());
            }
        } catch (const std::exception& e) {
            response["response"] = std::string("Error: ") + e.what();
            response["status"] = "500";
        }
    }
};

REGISTER_ACTION("update", UpdateTableHandler)