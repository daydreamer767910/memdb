#ifndef TransportClient_HPP
#define TransportClient_HPP

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include "transportmng.hpp"
#include "util/util.hpp"
#include "util/timer.hpp"
#include "tcpclient.hpp"


class TransportClient:public TcpClient , public IConnection {
private:
	TransportClient(const TransportClient&) = delete;
    TransportClient& operator=(const TransportClient&) = delete;

public:
	using ptr = std::shared_ptr<TransportClient>;

	TransportClient(boost::asio::io_context& io_context, const std::string& user, const std::string& pwd)
		: io_context_(io_context), 
		work_guard_(boost::asio::make_work_guard(io_context)),
		TcpClient(io_context),
		user_(user), passwd_(pwd) {
		tranportMng_ = TransportMng::get_instance();
		id_ = 1;
	}

	~TransportClient() {
		
	}

	void quit() {
		io_context_.stop();
		tranportMng_->on_close_item(transport_->get_id());
		tranportMng_->stop();
		if (asio_eventLoopThread.joinable())
			asio_eventLoopThread.join();
		work_guard_.reset();
	}

	static ptr& get_instance(boost::asio::io_context& io_context,const std::string& user, const std::string& pwd) {
		if (my_instance == nullptr)
        	my_instance = std::make_shared<TransportClient>(io_context,user,pwd);
		return my_instance;
    }

	int start(const std::string& host, const std::string& port);
	int reconnect(const std::string& host = "", const std::string& port = "");
	void stop();
	
	// 1. APP 缓存到下行 CircularBuffer
    int send(const uint8_t* data, size_t size, uint32_t msg_id, uint32_t timeout);
	// 4. APP 读取上行 CircularBuffer
    int recv(uint8_t* pack_data,uint32_t& msg_id, size_t size, uint32_t timeout);
	
	//for tcp client callback
	DataVariant& get_data() override {
		memset(write_buf,0,sizeof(write_buf));
        return cached_data_;
    }
	void on_data_received(int len,int ) override ;
	void set_transport(const std::shared_ptr<Transport>& transport) override {
        transport_ = transport;
        cached_data_ = std::make_tuple(write_buf, sizeof(write_buf), transport->get_id());
		transport_->setCompressFlag(true);
		transport_->setEncryptMode(false);
    }
	uint32_t get_id() {
        return id_;
    }
protected:
	int Ecdh();
	void handle_read(const boost::system::error_code& error, std::size_t nread) override;
private:
	boost::asio::io_context& io_context_;
	uint32_t id_;
	static ptr my_instance;
	std::string host_;
	std::string port_;
	TransportMng::ptr tranportMng_;
	std::shared_ptr<Transport> transport_;
	char read_buf[TCP_BUFFER_SIZE];
    char write_buf[TCP_BUFFER_SIZE];
    DataVariant cached_data_;
	std::thread asio_eventLoopThread;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
	std::string user_;
	std::string passwd_;
};

#endif