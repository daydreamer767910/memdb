#include "dbtask.hpp"
#include "dbservice.hpp"


void DbTask::on_data_received(int result) {
    static std::atomic<uint32_t> msg_id(0);
    if (result > 0) {
        auto json_datasCopy = std::make_shared<std::vector<json>>(json_datas_);
        auto self = shared_from_this();  // 确保对象生命周期
        boost::asio::post(io_context_, [self, this, json_datasCopy]() {
            this->handle_task(msg_id++, json_datasCopy);
        });
        json_datas_.clear();
    }
}


void DbTask::handle_task(uint32_t msg_id, std::shared_ptr<std::vector<json>> json_datas) {
	json jsonResp;
	//std::cout << "DbTask[" << port_id_ << "]:Pid[" << std::this_thread::get_id() << "]:handle_task" << std::endl;
	for (auto& jsonTask : *json_datas) {
		//std::cout << jsonTask.dump(4) << std::endl;
		std::string action = jsonTask["action"];
		auto db = DBService::getInstance()->getDb();
		try {
			if (action == "create") {	
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
					int num = tb->insertRowsFromJson(jsonTask);
					jsonResp["response"] = "insert table sucess";
					jsonResp["status"] = "200";
					jsonResp["rows inserted"] = num;
				} else {
					jsonResp["error"] = "table[" + tableName + "] not exist";
				}
			} else if(action == "show") {
				auto tables = db->listTables();
				for ( auto table: tables) {
					jsonResp[table->name] = table->showTable();
				}
			} else if(action == "get") {
				std::string tableName = jsonTask["name"];
				uint32_t limit = jsonTask["limit"];
				uint32_t offset = jsonTask["offset"];
				auto tb = db->getTable(tableName);
				if (tb) {
					jsonResp[tableName] = tb->rowsToJson(tb->getWithLimitAndOffset(limit,offset));
				} else {
					jsonResp["error"] = "table[" + tableName + "] not exist";
				}
			} else if(action == "count") {
				std::string tableName = jsonTask["name"];
				auto tb = db->getTable(tableName);
				if (tb) {
					jsonResp[tableName] = tableName;
					jsonResp["total"] = tb->getTotalRows();
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