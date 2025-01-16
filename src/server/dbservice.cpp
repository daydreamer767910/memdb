#include "dbservice.hpp"
#include "log/logger.hpp"
#include "net/transportsrv.hpp"
#include "util/util.hpp"

void DBService::on_msg(const std::shared_ptr<DBVariantMsg> msg) {
	static std::atomic<uint32_t> msg_id(0);
	std::visit([this](auto&& message) {
		std::lock_guard<std::mutex> lock(mutex_);
        auto [jsonDatas, port_id] = message;
		auto it = tasks_.find(port_id);
		if (it != tasks_.end()) {
			it->second->handle_task(msg_id++, jsonDatas);
		}
    }, *msg);
}

void DBService::keep_alive() {
	auto port_ids = TransportSrv::get_instance()->get_all_ports();
	std::cout << " Timer thread id: " << std::this_thread::get_id() <<std::endl;
	json jsonData;
	for (int port_id : port_ids) {
		jsonData["type"] = "keep alive(s)";
		jsonData["timeout"] = keep_alv_timer/1000;
		auto port = TransportSrv::get_instance()->get_port(port_id);
		if (port) {
			int ret = port->send(jsonData.dump(), 0xffffffff ,std::chrono::milliseconds(100));
			if (ret<0) {
				std::cout << "APP SEND err:" << ret <<  std::endl;
			}
		}
	}
}

void DBService::save_db() {
	std::string baseDir = std::string(std::getenv("HOME"));
	std::filesystem::path fullPath = std::filesystem::path(baseDir) / std::string("data");
//std::cout << "DBService::on_timer :save db to" << fullPath.string() << std::endl;
	db->save(fullPath.string());
}

void DBService::load_db() {
	std::string baseDir = std::string(std::getenv("HOME"));
	std::filesystem::path fullPath = std::filesystem::path(baseDir) / std::string("data");
	db->upload(fullPath.string());
}

void DBService::on_timer(int , int , std::thread::id ) {
	//save_db();
	//keep_alive();
}

