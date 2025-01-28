#include "../registry.hpp"

class SelectTableHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
        std::string name = task["name"];
		auto container = db->getContainer(name);
		// 准备响应
        response["response"] = "select container success";
        response["status"] = "200";
        
        if (container == nullptr) {
            response["response"] = "Container not found";
            response["status"] = "404";
            return;
        }
		try {
            if (container->getType() == "table") {
				uint32_t limit = task["limit"];
				std::vector<std::string> columnNames = task["columns"].get<std::vector<std::string>>();
				std::vector<std::string> conditions = task["conditions"].get<std::vector<std::string>>();
				std::vector<std::string> operators = task["ops"].get<std::vector<std::string>>();
				auto tb = std::dynamic_pointer_cast<Table>(container);
				if (!tb) {
                    throw std::runtime_error("Failed to cast to Table");
                }
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

				auto ret = tb->query(columnNames,conditions,queryValues,operators,limit);
				for (auto& fieldValues : ret) { //每一行数据
					json rowJson;
					for (size_t i = 0; i < columnNames.size(); ++i) { //每一列
						if (rowJson[columnNames[i]].is_null()) {
							rowJson[columnNames[i]] = json::array();
						}
						rowJson[columnNames[i]] = Field(fieldValues[i]).toJson();
					}
					response["results"].push_back(rowJson);
				}
				// 总行数
				response["total"] = ret.size();
            } else if (container->getType() == "collection") {
                auto collection = std::dynamic_pointer_cast<Collection>(container);
                if (!collection) {
                    throw std::runtime_error("Failed to cast to Collection");
                }
                auto results = collection->queryFromJson(task);
                json j = json::array();
                for (const auto& result : results) {
                    j.push_back(result->toJson());
                }
                response["results"] = j;
				response["total"] = results.size();
            } else {
                throw std::runtime_error("Unknown container type: " + container->getType());
            }
        } catch (const std::exception& e) {
            response["response"] = std::string("Error: ") + e.what();
            response["status"] = "500";
        }
    }
};

REGISTER_ACTION("select", SelectTableHandler)