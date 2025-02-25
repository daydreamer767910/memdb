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
    uint32_t msg_id;
    int ret = client_ptr->recv(reinterpret_cast<uint8_t*>(buffer), msg_id, size, timeout);
    return ret;
}

int mdb_send(const char* json_strdata, int msg_id, int timeout) {
	return client_ptr->send(std::string(json_strdata), msg_id, timeout);
}

}

