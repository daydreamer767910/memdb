#include "../registry.hpp"

class DropTableHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
        std::string tableName = task["name"];
        db->removeContainer(tableName);
        std::string baseDir = std::string(std::getenv("HOME"));
        std::filesystem::path fullPath = std::filesystem::path(baseDir) / std::string("data");
        db->remove(fullPath,tableName);
        response["response"] = "drop " + tableName + " success";
        response["status"] = "200";
    }
};

REGISTER_ACTION("drop", DropTableHandler)