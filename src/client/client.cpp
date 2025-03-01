#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "util/util.hpp"
#include "util/timer.hpp"
#include "net/transportclient.hpp"

// 创建 TcpClient 对象

boost::asio::io_context io_context;
TransportClient::ptr client_ptr = nullptr;

extern "C" {

void mdb_init(const char* user, const char* pwd) {
	client_ptr = TransportClient::get_instance(io_context,user,pwd);
}

int mdb_start(const char* ip, int port) {
	return client_ptr->start(std::string(ip), std::to_string(port));
}

int mdb_reconnect(const char* ip, int port) {
	client_ptr->stop();
	return client_ptr->reconnect(std::string(ip), std::to_string(port));
}

void mdb_stop() {
	client_ptr->stop();
}

void mdb_quit() {
	client_ptr->quit();
}

int mdb_recv(char* buffer, int size, int &msg_id, int timeout) {
	uint32_t id;
    int ret = client_ptr->recv(reinterpret_cast<uint8_t*>(buffer), id, size, timeout);
	if (ret > 0 ) msg_id = id;
    return ret;
}

int mdb_send(const char* json_strdata, int size, int msg_id, int timeout) {
	return client_ptr->send(reinterpret_cast<const uint8_t*>(json_strdata), size, msg_id, timeout);
}

}

