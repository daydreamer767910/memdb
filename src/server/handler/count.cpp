#include "../registry.hpp"

class CountTableHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
        std::string tableName = task["name"];
		auto container = db->getContainer(tableName);
        if (container->getType() == "table") {
			auto tb = std::dynamic_pointer_cast<Table>(container);
			response["total"] = tb->getTotalRows();
        } else if (container->getType() == "collection") {
            auto collection = std::dynamic_pointer_cast<Collection>(container);
			response["total"] = collection->getTotalDocument();
        }
		response["container"] = tableName;
        response["type"] = container->getType();
    }
};

REGISTER_ACTION("count", CountTableHandler)