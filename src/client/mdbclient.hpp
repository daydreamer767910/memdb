#ifndef MDBCLIENT_HPP
#define MDBCLIENT_HPP

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include "net/transportsrv.hpp"
#include "log/logger.hpp"
#include "util/util.hpp"
#include "util/timer.hpp"
#include "net/tcpclient.hpp"


class MdbClient:public TcpClient , public IDataCallback {
private:
	MdbClient(const MdbClient&) = delete;
    MdbClient& operator=(const MdbClient&) = delete;

public:
	using ptr = std::shared_ptr<MdbClient>;

	MdbClient(boost::asio::io_context& io_context )
		: io_context_(io_context), 
		work_guard_(boost::asio::make_work_guard(io_context)),
		TcpClient(io_context)  {
		transport_srv = TransportSrv::get_instance();
		std::cout << "MdbClient created" << std::endl;
	}

	~MdbClient() {
		work_guard_.reset();
	}

	static ptr& get_instance(boost::asio::io_context& io_context) {
		if (my_instance == nullptr)
        	my_instance = std::make_shared<MdbClient>(io_context);
		return my_instance;
    }

	void set_transport(trans_pair& port_info);

	uint32_t get_transportid();

	int start(const std::string& host, const std::string& port);
	void stop();
	// 1. APP 缓存到下行 CircularBuffer
    int send(const std::string& data, uint32_t msg_id, uint32_t timeout);
	// 4. APP 读取上行 CircularBuffer
    int recv(std::vector<json>& json_datas, size_t size, uint32_t timeout);
	int reconnect(const std::string& host = "", const std::string& port = "");
	DataVariant& get_data() override {
		memset(write_buf,0,sizeof(write_buf));
		cached_data_ = std::make_tuple(write_buf, sizeof(write_buf), transport_id_);
        return cached_data_;
    }
	void on_data_received(int result) override ;
protected:
	void handle_read(const boost::system::error_code& error, std::size_t nread) override;
private:
	boost::asio::io_context& io_context_;
	static ptr my_instance;
	std::string host_;
	std::string port_;
	TransportSrv::ptr transport_srv;
	std::shared_ptr<Transport> transport_;
	uint32_t transport_id_;
	char read_buf[TCP_BUFFER_SIZE];
    char write_buf[TCP_BUFFER_SIZE];
    DataVariant cached_data_;
	std::thread asio_eventLoopThread;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
};

#endif