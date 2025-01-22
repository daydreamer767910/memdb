#include "../registry.hpp"

class CreateTableHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
        std::string tableName = task["name"];
        // 假设 jsonToColumns 是一个已实现的工具函数
        auto columns = jsonToColumns(task);
        // 假设 DBService 是一个数据库服务的单例
        db->addTable(tableName, columns);
        response["response"] = "create " + tableName + " success";
        response["status"] = "200";
    }
};

REGISTER_ACTION("create", CreateTableHandler)