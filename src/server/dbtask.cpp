#include "dbtask.hpp"
#include "dbservice.hpp"

void DbTask::handle_task() {
	json jsonResp;
	for (auto jsonTask : json_datas) {
		std::string action = jsonTask["action"];
		auto db = DBService::getInstance()->getDb();
		try {
			if (action == "create table") {	
				std::string tableName = jsonTask["name"];
				auto columns = jsonToColumns(jsonTask);
				db->addTable(tableName, columns);
				jsonResp["response"] = "create table sucess";
				jsonResp["status"] = "200";
				//std::cout << jsonResp.dump(4) << std::endl;
			} else if(action == "insert") {
				std::string tableName = jsonTask["name"];
				auto tb = db->getTable(tableName);
				if (tb) {
					tb->insertRowsFromJson(jsonTask);
					jsonResp["response"] = "insert table sucess";
					jsonResp["status"] = "200";
				} else {
					jsonResp["error"] = "table[" + tableName + "] not exist";
				}
			} else if(action == "show tables") {
				auto tables = db->listTables();
				for ( auto table: tables) {
					jsonResp[table->name] = table->showTable();
				}
			} else if(action == "get") {
				std::string tableName = jsonTask["name"];
				auto tb = db->getTable(tableName);
				if (tb) {
					jsonResp[tableName] = tb->showRows();
				} else {
					jsonResp["error"] = "table[" + tableName + "] not exist";
				}
			} else if(action == "update") {

			}
		} catch (const std::runtime_error& e) {
			jsonResp["error"] = e.what();
		} catch (const std::exception& e) {
			jsonResp["error"] = e.what();
		} catch (...) {
			std::cerr << "Unknown exception occurred!" << std::endl;
			jsonResp["error"] = "Unknown exception occurred!";
		}
		auto msg = std::make_shared<DBMsg>(std::make_tuple(1,port_id_,jsonResp));
		DBService::getInstance()->sendmsg(msg);
	}
	
}