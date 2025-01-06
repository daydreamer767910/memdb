#include "dbservice.hpp"
#include "log/logger.hpp"
#include "net/transportsrv.hpp"
#include "dbcore/memdatabase.hpp"

DBService& DBService::get_instance() {
    static DBService instance;
    return instance;
}

void DBService::on_msg(const std::shared_ptr<DBMsg> msg) {
	std::visit([this](auto&& message) {
		auto [msg_type,transport] = message;
		//logger.log(Logger::LogLevel::INFO, "dbservice on_msg {} {}",msg_type,transport);
		auto channel_ptr = TransportSrv::get_instance().get_transport(transport);
		if (msg_type == 1 && channel_ptr) { 
			json jsonData;
			MemDatabase::ptr& db = MemDatabase::getInstance();
			auto users = db->getTable("users");
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
			}
		}
	}, *msg);
}

void DBService::process() {
	while (true) {
		json jsonData;
		auto port_ids = TransportSrv::get_instance().get_all_ports();
		if (this->is_terminate()) {
			break;  // 退出线程
		}
		for (int port_id : port_ids) {
			auto channel_ptr = TransportSrv::get_instance().get_transport(port_id);
			if (!channel_ptr) continue;
			int ret = channel_ptr->appReceive(jsonData, std::chrono::milliseconds(1000));
			if (ret<0) {
				//logger.log(Logger::LogLevel::WARNING, "appReceive fail {}", ret);
				if (ret == -2) {
					channel_ptr->resetBuffers();
				}
			} else {
				//parse the json
				std::cout << "APP RECV:" << jsonData.dump(4) << std::endl;
			}
		}
	}
}

// 处理事务
void DBService::on_timer() {
	auto port_ids = TransportSrv::get_instance().get_all_ports();

	for (int port_id : port_ids) {
		//this->sendmsg(std::make_shared<DBMsg>(std::make_tuple(1,port_id)));
		this->on_msg(std::make_shared<DBMsg>(std::make_tuple(1,port_id)));
	}	
}