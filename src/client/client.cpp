#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "net/transportsrv.hpp"
#include "util/util.hpp"
#include "util/timer.hpp"

#include "mdbclient.hpp"

// 创建 TcpClient 对象

boost::asio::io_context io_context;
MdbClient::ptr client_ptr = nullptr;
Database::ptr& db = Database::getInstance();
extern "C" {

void mdb_init() {
	std::string baseDir = std::string(std::getenv("HOME"));
	std::filesystem::path fullPath = std::filesystem::path(baseDir) / std::string("data");
	db->upload(fullPath.string());
	client_ptr = MdbClient::get_instance(io_context);
}

int mdb_start(const char* ip, int port) {
	return client_ptr->start(std::string(ip), std::to_string(port));
}

int mdb_reconnect(const char* ip, int port) {
	return client_ptr->reconnect(std::string(ip), std::to_string(port));
}

void mdb_stop() {
	client_ptr->stop();
}

void mdb_quit() {
	client_ptr->quit();
}

int mdb_recv(char* buffer, int size, int timeout) {
	std::vector<json> json_datas;
	int ret = client_ptr->recv(json_datas, 1, timeout);
	if (ret>0) {
		std::string json_strdata;
		for (auto json_data : json_datas) {
			json_strdata += json_data.dump();
		}
		ret = std::min(size_t(size),json_strdata.size());
		memcpy(buffer, json_strdata.c_str(), ret);
	}
	return ret;
}

int mdb_send(const char* json_strdata, int msg_id, int timeout) {
	return client_ptr->send(std::string(json_strdata), msg_id, timeout);
}

}

