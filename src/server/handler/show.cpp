#include "../registry.hpp"

class ShowTableHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
        std::string tableName = task["name"];
        auto tables = db->listContainers();
		for ( auto table: tables) {
			if (tableName == "*" || tableName == table->getName()) {
				response[table->getName()] = table->toJson();
			}
		}
    }
};

REGISTER_ACTION("show", ShowTableHandler)