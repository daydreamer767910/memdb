#include "dbservice.hpp"
#include "log/logger.hpp"
#include "net/transportsrv.hpp"
#include "util/util.hpp"

void DBService::on_msg(const std::shared_ptr<DBMsg> msg) {
	static uint32_t msg_id = 0;
	std::visit([this](auto&& message) {
		auto [msg_type,transport,jsonData] = message;
		//logger.log(Logger::LogLevel::INFO, "dbservice on_msg {} {}",msg_type,transport);
		if (msg_type == 1 ) { 
			auto port = TransportSrv::get_instance()->get_port(transport);
			if (port) {
				int ret = port->send(jsonData.dump(), msg_id++ ,std::chrono::milliseconds(100));
				if (ret<0) {
					logger.log(Logger::LogLevel::WARNING, "appSend fail, data full");
				} else {
					//std::cout << "APP SEND:" << get_timestamp() << "\n" << jsonData.dump(4) << std::endl;
				}
			}
		} else if (msg_type == 0 ) {
			auto port = TransportSrv::get_instance()->get_port(transport);
			if (port) {
				int ret = port->send(jsonData.dump(), 0xffffffff ,std::chrono::milliseconds(100));
				if (ret<0) {
					logger.log(Logger::LogLevel::WARNING, "appSend fail, data full");
				} else {
					//std::cout << "APP SEND:" << get_timestamp() << "\n" << jsonData.dump(4) << std::endl;
				}
			}
		}
	}, *msg);
}

void DBService::on_timer() {
	auto port_ids = TransportSrv::get_instance()->get_all_ports();

	json jsonData;
	for (int port_id : port_ids) {
		jsonData["type"] = "keep alive(s)";
		jsonData["timer"] = keep_alv_timer/1000;
		this->on_msg(std::make_shared<DBMsg>(std::make_tuple(0,port_id,jsonData)));
	}
}

