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

extern "C" {

void mdb_init(const char* user, const char* pwd) {
	client_ptr = MdbClient::get_instance(io_context,user,pwd);
}

int mdb_start(const char* ip, int port) {
	int ret = client_ptr->start(std::string(ip), std::to_string(port));
	if (ret<0) return ret;
	client_ptr->setEncryptMode(false);
	return client_ptr->Ecdh();
}

int mdb_reconnect(const char* ip, int port) {
	int ret = client_ptr->reconnect(std::string(ip), std::to_string(port));
	if (ret<0) return ret;
	client_ptr->setEncryptMode(false);
	return client_ptr->Ecdh();
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
		if (size < json_strdata.size()) {
			std::cerr << "recv buffer size is too small!!\n";
			return -5;
		}
		//ret = std::min(size_t(size),json_strdata.size());
		memcpy(buffer, json_strdata.c_str(), json_strdata.size());
	}
	return ret;
}

int mdb_send(const char* json_strdata, int msg_id, int timeout) {
	return client_ptr->send(std::string(json_strdata), msg_id, timeout);
}

}

