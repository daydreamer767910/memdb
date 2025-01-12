#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "net/transportsrv.hpp"
#include "log/logger.hpp"
#include "util/util.hpp"
#include "util/timer.hpp"
#include "tcpclient.hpp"
#include "mdbclient.hpp"

// 创建 TcpClient 对象

boost::asio::io_context io_context;

extern "C" {

MdbClient::ptr init() {
	return MdbClient::get_instance(io_context);
}

}

