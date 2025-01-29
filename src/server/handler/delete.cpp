#include "../registry.hpp"

class DeleteTableHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
		std::string name = task["name"];
        auto container = db->getContainer(name);
        
        // 准备响应
        response["response"] = "delete container success";
        response["status"] = "200";
        
        if (container == nullptr) {
            response["response"] = "Container not found";
            response["status"] = "404";
            return;
        }
        try {
            if (container->getType() == "table") {
				auto tb = std::dynamic_pointer_cast<Table>(container);
                if (!tb) {
                    throw std::runtime_error("Failed to cast to Table");
                }
				std::vector<std::string> conditions = task["conditions"].get<std::vector<std::string>>();
				std::vector<std::string> operators = task["ops"].get<std::vector<std::string>>();
				std::vector<FieldType> qtypes = tb->getColumnTypes(conditions);
				if (qtypes.size() != task["qvalues"].size()) {
					throw std::invalid_argument("Mismatch between types and values count");
				}
				std::vector<FieldValue> queryValues;
				queryValues.reserve(qtypes.size());
				for (size_t i = 0; i < qtypes.size(); ++i) {
					Field field;
					field.fromJson(task["qvalues"][i]);
					if (!field.typeMatches(qtypes[i])) {
						throw std::invalid_argument("Mismatch type between types and values");
					}
					queryValues.push_back(field.getValue());
				}
				size_t removed = tb->remove(conditions,queryValues,operators);
				response["deleted"] = removed;
			} else if (container->getType() == "collection") {
                auto collection = std::dynamic_pointer_cast<Collection>(container);
                if (!collection) {
                    throw std::runtime_error("Failed to cast to Collection");
                }
				response["deleted"] = collection->deleteFromJson(task);
			}
		} catch (const std::exception& e) {
            response["response"] = std::string("Error: ") + e.what();
            response["status"] = "500";
        }

    }
};

REGISTER_ACTION("delete", DeleteTableHandler)