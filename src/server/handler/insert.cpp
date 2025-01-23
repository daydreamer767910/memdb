#include "../registry.hpp"

class InsertTableHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
        std::string tableName = task["name"];
        auto tb = db->getTable(tableName);
        int rowsInserted = tb->insertRowsFromJson(task);
        response["response"] = "insert table success";
        response["status"] = "200";
        response["rows_inserted"] = rowsInserted;
    }
};

REGISTER_ACTION("insert", InsertTableHandler)