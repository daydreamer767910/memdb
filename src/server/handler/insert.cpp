#include "../registry.hpp"

class InsertTableHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
        std::string name = task["name"];
        auto tb = db->getContainer(name);
        int rowsInserted = 0;
        if (tb->getType() == "table") {
            rowsInserted = std::dynamic_pointer_cast<Table>(tb)->insertRowsFromJson(task);
        } else if (tb->getType() == "collection") {
            rowsInserted = std::dynamic_pointer_cast<Collection>(tb)->insertDocumentsFromJson(task);
        }
        response["response"] = "insert table success";
        response["status"] = "200";
        response["rows_inserted"] = rowsInserted;
    }
};

REGISTER_ACTION("insert", InsertTableHandler)