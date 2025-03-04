#include "transportsrv.hpp"

TransportSrv::TransportSrv()
    : io_context_(), acceptor_(io_context_), connection_count(0), unique_id(0) {
    tranportMng_ = TransportMng::get_instance();
}

void TransportSrv::tcp_start(const std::string& ip, int port) {
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

    io_context_.run();
}
void TransportSrv::start(const std::string& ip, int port) {
    tranportMng_->start();
	tcp_start(ip, port);
}

void TransportSrv::stop() {
    io_context_.stop();
    cleanup();
    tranportMng_->stop();
    #ifdef DEBUG
    std::cout << "TransportSrv stopped." << std::endl;
    #endif
}

void TransportSrv::on_accept(const boost::system::error_code& error, boost::asio::ip::tcp::socket socket) {
    if (error) {
        logger.log(Logger::LogLevel::ERROR, "New connection error: {}", error.message());
        return;
    }

    if (connection_count < max_connection_num) {
        connection_count++;
        unique_id++;
        auto& io_context = static_cast<boost::asio::io_context&>(acceptor_.get_executor().context());
		auto connection = std::make_shared<TcpConnection>(io_context, std::move(socket), unique_id);
        connection->add_observer(shared_from_this());
        connection->start();
    } else {
        socket.close();
    }

    // Accept next connection
    acceptor_.async_accept([this](const boost::system::error_code& error, boost::asio::ip::tcp::socket socket) {
        on_accept(error, std::move(socket));
    });
}

void TransportSrv::cleanup() {
    for (auto& [id, connection] : connections_) {
        connection->stop();
    }
    connections_.clear();
    connection_count = 0;
}
