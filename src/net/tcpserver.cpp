#include "tcpserver.hpp"
#include "log/logger.hpp"
#include "transport.hpp"
#include "transportsrv.hpp"

TcpServer::TcpServer(const std::string& ip, int port)
    : io_context_(), acceptor_(io_context_), connection_count(0), unique_id(0) {
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(ip), port);
    
    // 设置 SO_REUSEADDR 选项，允许重新绑定端口
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    
    acceptor_.bind(endpoint);
    acceptor_.listen(boost::asio::socket_base::max_listen_connections);
    
    logger.log(Logger::LogLevel::INFO, "TCP server listening on {}:{}", ip, port);

    // Timer for periodic cleanup
    timer_ = std::make_shared<boost::asio::steady_timer>(io_context_, boost::asio::chrono::seconds(1));
    timer_->async_wait([this](const boost::system::error_code&) {
        on_timer();
    });
}


void TcpServer::start() {
    acceptor_.async_accept([this](const boost::system::error_code& error, boost::asio::ip::tcp::socket socket) {
        on_new_connection(error, std::move(socket));
    });
    io_context_.run();
}

void TcpServer::stop() {
    cleanup();
    io_context_.stop();
}

void TcpServer::on_new_connection(const boost::system::error_code& error, boost::asio::ip::tcp::socket socket) {
    if (error) {
        logger.log(Logger::LogLevel::ERROR, "New connection error: {}", error.message());
        return;
    }

    if (connection_count < max_connection_num) {
        connection_count++;
        unique_id++;
        auto connection = std::make_shared<TcpConnection>(acceptor_.get_executor().context(), std::move(socket));
        connections_.emplace(unique_id, connection);
		TransportSrv::get_instance()->on_new_connection(connection);
        connection->start();
        logger.log(Logger::LogLevel::INFO, "New connection [{}] established", unique_id.load());
    } else {
        socket.close();
    }

    // Accept next connection
    acceptor_.async_accept([this](const boost::system::error_code& error, boost::asio::ip::tcp::socket socket) {
        on_new_connection(error, std::move(socket));
    });
}

void TcpServer::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, connection] : connections_) {
        connection->stop();
    }
    connections_.clear();
    connection_count = 0;
}

void TcpServer::on_timer() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = connections_.begin(); it != connections_.end();) {
        if (it->second->is_idle()) {
            connection_count--;
			TransportSrv::get_instance()->close_port(it->second->transport_id_);
            it->second->stop();
            logger.log(Logger::LogLevel::INFO, "Connection [{}] removed due to inactivity", it->first);
            it = connections_.erase(it);
        } else {
            ++it;
        }
    }
    timer_->expires_at(timer_->expiry() + boost::asio::chrono::seconds(1));
    timer_->async_wait([this](const boost::system::error_code&) {
        on_timer();
    });
}
