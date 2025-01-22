#include "../registry.hpp"

class ShowTableHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
        std::string tableName = task["name"];
        auto tables = db->listTables();
		for ( auto table: tables) {
			if (tableName == "*" || tableName == table->name_) {
				response[table->name_] = table->showTable();
			}
		}
    }
};

REGISTER_ACTION("show", ShowTableHandler)