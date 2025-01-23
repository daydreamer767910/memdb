#include "../registry.hpp"

class GetHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
        std::string tableName = task["name"];
		uint32_t limit = task["limit"];
		uint32_t offset = task["offset"];
		auto tb = db->getTable(tableName);
		response[tableName] = tb->rowsToJson(tb->getWithLimitAndOffset(limit,offset));
    }
};

REGISTER_ACTION("get", GetHandler)