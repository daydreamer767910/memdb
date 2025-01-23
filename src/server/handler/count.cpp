#include "../registry.hpp"

class CountTableHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
        std::string tableName = task["name"];
		auto tb = db->getTable(tableName);
		response[tableName] = tableName;
		response["total"] = tb->getTotalRows();
    }
};

REGISTER_ACTION("count", CountTableHandler)