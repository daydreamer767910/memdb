#ifndef TransportSrv_HPP
#define TransportSrv_HPP

#include <boost/asio.hpp>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <mutex>
#include "tcpconnection.hpp"
#include "log/logger.hpp"
#include "transportmng.hpp"

#define MAX_CONNECTIONS 1000

class TransportSrv: public IObserver<TcpConnection>, public Subject<Transport>, public std::enable_shared_from_this<TransportSrv> {
public:
    using ptr = std::shared_ptr<TransportSrv>;
    static ptr& get_instance() {
		static ptr instance = std::make_shared<TransportSrv>();
		return instance;
	}
    TransportSrv();
    ~TransportSrv() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            #ifdef DEBUG
            std::cout << "TransportSrv destroyed!" << std::endl;
            #endif
            connections_.clear();
        }
    }
    void start(const std::string& ip, int port);
    void stop();
    void on_new_item(const std::shared_ptr<TcpConnection>& connection, uint32_t id) override {
		std::lock_guard<std::mutex> lock(mutex_);
        connections_.emplace(unique_id, connection);
        auto port = tranportMng_->open_port(id);
		port->add_callback(connection);
		connection->set_transport(port);
		notify_new_item(port, id);
		logger.log(Logger::LogLevel::INFO, "New connection [{}] established", id);
    }

	void on_close_item(const uint32_t id) override {
		std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(id);
        if ( it != connections_.end()) {
            connection_count--;
            it = connections_.erase(it);
            tranportMng_->close_port(id);
		    notify_close_item(id);
			logger.log(Logger::LogLevel::INFO, "Connection [{}] removed", id);
        }
    }

private:
    void on_accept(const boost::system::error_code& error, boost::asio::ip::tcp::socket socket);
    void cleanup();
	void tcp_start(const std::string& ip, int port);
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::unordered_map<uint32_t, std::shared_ptr<TcpConnection>> connections_;
    std::mutex mutex_;
    std::atomic<int> connection_count;
    std::atomic<uint32_t> unique_id;
    uint32_t max_connection_num = MAX_CONNECTIONS;
    TransportMng::ptr tranportMng_;
};

#endif