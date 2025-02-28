#include "tcpserver.hpp"
#include "log/logger.hpp"

TcpServer::TcpServer()
    : io_context_(), acceptor_(io_context_), connection_count(0), unique_id(0) ,
	timer_(io_context_, 1000, true, [this](int /*tick*/, int /*time*/, std::thread::id id) {
        this->on_timer(id);
    }) {
    
}

void TcpServer::start(const std::string& ip, int port) {
	boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(ip), port);
    // 设置 SO_REUSEADDR 选项，允许重新绑定端口
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    
    acceptor_.bind(endpoint);
	acceptor_.listen(boost::asio::socket_base::max_listen_connections);
    
    logger.log(Logger::LogLevel::INFO, "TCP server listening on {}:{}", ip, port);

    acceptor_.async_accept([this](const boost::system::error_code& error, boost::asio::ip::tcp::socket socket) {
        on_accept(error, std::move(socket));
    });
	timer_.start();
    io_context_.run();
}

void TcpServer::stop() {
    timer_.stop();
    io_context_.stop();
    cleanup();
    #ifdef DEBUG
    std::cout << "TcpServer stopped." << std::endl;
    #endif
}

void TcpServer::on_accept(const boost::system::error_code& error, boost::asio::ip::tcp::socket socket) {
    if (error) {
        logger.log(Logger::LogLevel::ERROR, "New connection error: {}", error.message());
        return;
    }

    if (connection_count < max_connection_num) {
        connection_count++;
        unique_id++;
        auto& io_context = static_cast<boost::asio::io_context&>(acceptor_.get_executor().context());
		auto connection = std::make_shared<TcpConnection>(io_context, std::move(socket), unique_id);
        connections_.emplace(unique_id, connection);
		on_new_connection(connection, unique_id);
        connection->start();
        logger.log(Logger::LogLevel::INFO, "New connection [{}] established", unique_id.load());
    } else {
        socket.close();
    }

    // Accept next connection
    acceptor_.async_accept([this](const boost::system::error_code& error, boost::asio::ip::tcp::socket socket) {
        on_accept(error, std::move(socket));
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

void TcpServer::on_timer(std::thread::id /*id*/) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = connections_.begin(); it != connections_.end();) {
        if (it->second->is_idle()) {
            connection_count--;
			on_close_connection(it->second->get_id());
            it->second->stop();
            logger.log(Logger::LogLevel::INFO, "Connection [{}] removed due to inactivity", it->first);
            it = connections_.erase(it);
        } else {
            ++it;
        }
    }
}
