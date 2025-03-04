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
#include "tcpconnection.hpp"


class TransportClient: public IObserver<TcpConnection>,public std::enable_shared_from_this<TransportClient> {
private:
	TransportClient(const TransportClient&) = delete;
    TransportClient& operator=(const TransportClient&) = delete;

public:
	using ptr = std::shared_ptr<TransportClient>;

	TransportClient(boost::asio::io_context& io_context, const std::string& user, const std::string& pwd)
		: io_context_(io_context), 
		work_guard_(boost::asio::make_work_guard(io_context)),
		user_(user), passwd_(pwd) {
		tranportMng_ = TransportMng::get_instance();
		id_ = 0;
		tcp_client_ = nullptr;
	}

	~TransportClient() {
		
	}
	void on_new_item(const std::shared_ptr<TcpConnection>& connection, uint32_t id) override {
		auto transport = tranportMng_->open_port(id_);
    	transport->add_callback(tcp_client_);
		transport->setCompressFlag(true);
		transport->setEncryptMode(false);
		transport->reset(Transport::ChannelType::ALL);
		connection->set_transport(transport);
		transport_ = transport;
    }

	void on_close_item(const uint32_t id) override {
        tranportMng_->close_port(id);
		tcp_client_ = nullptr;
    }
	//for tcp client
    std::optional<tcp::socket> connect(const std::string& host, const std::string& port) {
        try {
            tcp::resolver resolver(io_context_);
            auto endpoints = resolver.resolve(host, port);

            tcp::socket socket(io_context_);
            boost::asio::connect(socket, endpoints);

            #ifdef DEBUG
            std::cout << "Connected to " << host << ":" << port << std::endl;
            #endif

            return std::move(socket);  // 使用 std::move 进行移动返回
        } catch (const std::exception& e) {
            std::cerr << "Error connecting to server: " << e.what() << std::endl;
            return std::nullopt;  // 返回空值表示连接失败
        }
    }

	void quit() {
		io_context_.stop();
		if (auto port = transport_.lock()) {
			tranportMng_->close_port(port->get_id());
		}
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
	
protected:
	int Ecdh();
private:
	static ptr my_instance;
	boost::asio::io_context& io_context_;
	std::string host_;
	std::string port_;
	TransportMng::ptr tranportMng_;
	uint32_t id_;
	std::thread asio_eventLoopThread;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
	std::string user_;
	std::string passwd_;
	std::shared_ptr<TcpConnection> tcp_client_;
	std::weak_ptr<Transport> transport_;
};

#endif