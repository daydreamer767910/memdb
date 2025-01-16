#include "dbtask.hpp"
#include "dbservice.hpp"


void DbTask::on_data_received(int result) {
	if (result >0 ) {
		auto msg = std::make_shared<DBVariantMsg>(std::make_tuple(json_datas,port_id_));
		DBService::getInstance()->sendmsg(msg);
	}
}

void DbTask::handle_task(uint32_t msg_id, std::vector<json>& json_datasCopy) {
	json jsonResp;
	//std::cout << "DbTask handle_task:" << std::this_thread::get_id() << std::endl;
	for (auto& jsonTask : json_datasCopy) {
		//std::cout << jsonTask.dump(4) << std::endl;
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
		auto port = TransportSrv::get_instance()->get_port(port_id_);
		if (port) {
			int ret = port->send(jsonResp.dump(), msg_id ,std::chrono::milliseconds(100));
			if (ret<0) {
				std::cout << "APP SEND err:" << ret <<  std::endl;
			}
		}
		
	}
	
}