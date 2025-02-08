#include "../registry.hpp"

class InsertTableHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db, json& response) override {
        std::string name = task["name"];
        auto container = db->getContainer(name);
        
        // 准备响应
        response["response"] = "insert container success";
        response["status"] = "200";
        
        if (container == nullptr) {
            response["response"] = "Container not found";
            response["status"] = "404";
            return;
        }
        
        try {
            if (container->getType() == "table") {
                auto table = std::dynamic_pointer_cast<Table>(container);
                if (!table) {
                    throw std::runtime_error("Failed to cast to Table");
                }
                
                int rowsInserted = table->insertRowsFromJson(task);
                response["rows_inserted"] = rowsInserted;
            } else if (container->getType() == "collection") {
                auto collection = std::dynamic_pointer_cast<Collection>(container);
                if (!collection) {
                    throw std::runtime_error("Failed to cast to Collection");
                }
                
                std::vector<DocumentId> insertedIds = collection->insertDocumentsFromJson(task);
                json ids = json::array();
                for (const auto& id : insertedIds) {
                    ids.push_back({{"_id", id}});
                }
                response["rows_inserted"] = ids;
            } else {
                throw std::runtime_error("Unknown container type: " + container->getType());
            }
        } catch (const std::exception& e) {
            response["response"] = std::string("Error: ") + e.what();
            response["status"] = "500";
        }
    }

};

REGISTER_ACTION("insert", InsertTableHandler)