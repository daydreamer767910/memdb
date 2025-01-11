#ifndef MDBCLIENT_HPP
#define MDBCLIENT_HPP

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


class MdbClient:public TcpClient , public IDataCallback {
using ptr = std::shared_ptr<MdbClient>;

private:
	MdbClient(const MdbClient&) = delete;
    MdbClient& operator=(const MdbClient&) = delete;

public:
	MdbClient(boost::asio::io_context& io_context, const std::string& host, const std::string& port)
		: TcpClient(io_context) , host_(host),port_(port) {
		std::cout << "MdbClient created" << std::endl;
	}
	static ptr create(boost::asio::io_context& io_context,
                                         const std::string& host,
                                         const std::string& port) {
		return ptr(new MdbClient(io_context, host, port));
	}

	static void start(ptr client_ptr);
	void stop();
	// 1. APP 缓存到下行 CircularBuffer
    int send(const json& json_data, uint32_t msg_id, uint32_t timeout);
	// 4. APP 读取上行 CircularBuffer
    int recv(std::vector<json>& json_datas, uint32_t timeout);

	DataVariant& get_data() override {
		memset(write_buf,0,sizeof(write_buf));
		cached_data_ = std::make_tuple(write_buf, sizeof(write_buf), transport_id_);
        return cached_data_;
    }
	void on_data_received(int result) override ;

private:
	std::string host_;
	std::string port_;
	std::shared_ptr<TransportSrv> transport_srv_;
	std::shared_ptr<Transport> transport_;
	uint32_t transport_id_;
	char read_buf[TCP_BUFFER_SIZE];
    char write_buf[TCP_BUFFER_SIZE];
    DataVariant cached_data_;
};

#endif