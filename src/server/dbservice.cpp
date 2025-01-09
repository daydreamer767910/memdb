#include "dbservice.hpp"
#include "log/logger.hpp"
#include "net/transportsrv.hpp"
#include "util/util.hpp"


DBService& dbSrv = DBService::get_instance(); // 定义全局变量

DBService& DBService::get_instance() {
    static DBService instance;
    return instance;
}

void DBService::handle_task(int port_id, json jsonTask) {
	json jsonResp;
	std::string action = jsonTask["action"];
	//logger.log(Logger::LogLevel::INFO, "DBService::handle_task begin +++++++");
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
	this->on_msg(std::make_shared<DBMsg>(std::make_tuple(1,port_id,jsonResp)));
	//logger.log(Logger::LogLevel::INFO, "DBService::handle_task end .......");
}

void DBService::on_msg(const std::shared_ptr<DBMsg> msg) {
	std::visit([this](auto&& message) {
		auto [msg_type,transport,jsonData] = message;
		//logger.log(Logger::LogLevel::INFO, "dbservice on_msg {} {}",msg_type,transport);
		if (msg_type == 1 ) { 
			int ret = TransportSrv::get_instance().send(jsonData, transport,1000);
			if (ret<0) {
				logger.log(Logger::LogLevel::WARNING, "appSend fail, data full");
			} else {
				//std::cout << "APP SEND:" << get_timestamp() << "\n" << jsonData.dump(4) << std::endl;
			}
			
			/*auto users = db->getTable("users");
			Field userName = std::string("Alice");
			const auto& queryRows = users->queryByIndex("name", userName);
			jsonData = users->rowsToJson(queryRows);
			//std::cout << "query Alice:" << jsonData.dump(4) << std::endl;
			// send to CircularBuffer
			int ret = channel_ptr->appSend(jsonData, transport, std::chrono::milliseconds(1000));
			if (ret<0) {
				logger.log(Logger::LogLevel::WARNING, "appSend fail, data full");
			} else {
				//std::cout << "appSend: " << ret << std::endl;
			}*/
		} else if (msg_type == 0 ) {
		}
	}, *msg);
}

void DBService::process() {
	static auto start = std::chrono::high_resolution_clock::now();
	while (true) {
		//json jsonData;
		std::vector<json> json_datas;
		auto port_ids = TransportSrv::get_instance().get_all_ports();
		if (this->is_terminate()) {
			break;  // 退出线程
		}
		for (int port_id : port_ids) {
			//logger.log(Logger::LogLevel::INFO, "DBService::process:transport:recv begin-------");
			auto ret = TransportSrv::get_instance().recv(json_datas,port_id,1000);
			//logger.log(Logger::LogLevel::INFO, "DBService::process:transport:recv end=======");
			if (ret == 0) continue;
			else if (ret<0) {
				//logger.log(Logger::LogLevel::WARNING, "appReceive fail {}", ret);
			} else {
				//parse the json
				for (auto jsonData :json_datas) {
					//std::cout << "APP RECV:" << get_timestamp() << "\n" << jsonData.dump(4) << std::endl;
					this->handle_task(port_id,jsonData);
				}
			}
		}
		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
		if (duration.count() > 2000) {
			json jsonData;
			for (int port_id : port_ids) {
				jsonData["timer"] = "2s timer";
				this->on_msg(std::make_shared<DBMsg>(std::make_tuple(0,port_id,jsonData)));
			}
			start = end;
		}
	}
}

