#include "../registry.hpp"

class GetHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
        std::string name = task["name"];
		uint32_t limit = task["limit"];
		uint32_t offset = task["offset"];
        auto container = db->getContainer(name);
        if (container->getType() == "table") {
			auto tb = std::dynamic_pointer_cast<Table>(container);
			response[name] = tb->rowsToJson(tb->getWithLimitAndOffset(limit,offset));
        } else if (container->getType() == "collection") {
            auto collection = std::dynamic_pointer_cast<Collection>(container);
			response[name] = collection->toJson();
        }
    }
};

REGISTER_ACTION("get", GetHandler)