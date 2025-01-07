#include "dbservice.hpp"
#include "log/logger.hpp"
#include "net/transportsrv.hpp"
#include "dbcore/memdatabase.hpp"

DBService& DBService::get_instance() {
    static DBService instance;
    return instance;
}

void DBService::handle_task(int port_id, json jsonTask) {
	json jsonResp;
	std::string action = jsonTask["action"];
	if (action == "create table") {
		try {
			MemDatabase::ptr& db = MemDatabase::getInstance();
			std::string tableName = jsonTask["name"];
			auto columns = jsonToColumns(jsonTask);
			db->addTable(tableName, columns);

			auto tables = db->listTables();
			for ( auto table: tables) {
				jsonResp[table->name] = table->tableToJson();
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
		//std::cout << jsonResp.dump(4) << std::endl;
	}
}

void DBService::on_msg(const std::shared_ptr<DBMsg> msg) {
	std::visit([this](auto&& message) {
		auto [msg_type,transport,jsonData] = message;
		//logger.log(Logger::LogLevel::INFO, "dbservice on_msg {} {}",msg_type,transport);
		auto channel_ptr = TransportSrv::get_instance().get_transport(transport);
		if (msg_type == 1 && channel_ptr) { 
			int ret = channel_ptr->appSend(jsonData, transport, std::chrono::milliseconds(1000));
			if (ret<0) {
				logger.log(Logger::LogLevel::WARNING, "appSend fail, data full");
			} else {
				//std::cout << "appSend: " << ret << std::endl;
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
		} else if (msg_type == 0 && channel_ptr) {
			int ret = channel_ptr->appSend(jsonData, transport, std::chrono::milliseconds(1000));
			if (ret<0) {
				logger.log(Logger::LogLevel::WARNING, "appSend fail, data full");
			} else {
				//std::cout << "appSend: " << ret << std::endl;
			}
		}
	}, *msg);
}

void DBService::process() {
	static auto start = std::chrono::high_resolution_clock::now();
	while (true) {
		json jsonData;
		auto port_ids = TransportSrv::get_instance().get_all_ports();
		if (this->is_terminate()) {
			break;  // 退出线程
		}
		for (int port_id : port_ids) {
			auto channel_ptr = TransportSrv::get_instance().get_transport(port_id);
			if (!channel_ptr) continue;
			int ret = channel_ptr->appReceive(jsonData, std::chrono::milliseconds(50));
			if (ret<0) {
				//logger.log(Logger::LogLevel::WARNING, "appReceive fail {}", ret);
				if (ret == -2) {
					channel_ptr->resetBuffers(Transport::BufferType::TCP2APP);
				}
			} else {
				//parse the json
				std::cout << "APP RECV:" << jsonData.dump(4) << std::endl;
				this->handle_task(port_id,jsonData);
			}
		}
		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
		if (duration.count() > 2000) {
			for (int port_id : port_ids) {
				jsonData["timer"] = "2s timer";
				this->on_msg(std::make_shared<DBMsg>(std::make_tuple(0,port_id,jsonData)));
			}
			start = end;
		}
	}
}

