#include "../registry.hpp"

class DropIdexesHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
        std::string name = task["name"];
		std::vector<std::string> indexes;
		for (const auto& item : task["indexes"]) {
			indexes.push_back(item.get<std::string>());
		}

        auto container = db->getContainer(name);
		// 准备响应
        response["response"] = "drop indexes ok";
        response["status"] = "200";
        
        if (container == nullptr) {
            response["response"] = "Container not found";
            response["status"] = "404";
            return;
        }
		try {
			if (container->getType() == "table") {
				auto tb = std::dynamic_pointer_cast<Table>(container);
				for (auto& idx: indexes) {
					tb->dropIndex(idx);
				}
			} else if (container->getType() == "collection") {
				auto collection = std::dynamic_pointer_cast<Collection>(container);
				for (auto& idx: indexes) {
					collection->dropIndex(idx);
				}
			}
		} catch (const std::exception& e) {
            response["response"] = std::string("Error: ") + e.what();
            response["status"] = "500";
        }
    }
};

REGISTER_ACTION("drop_idx", DropIdexesHandler)