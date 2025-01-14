#include "tcpconnection.hpp"
#include "log/logger.hpp"
#include "util/util.hpp"

TcpConnection::TcpConnection(boost::asio::io_context& io_context, tcp::socket socket)
    : io_context_(io_context), 
      socket_(std::move(socket)),
      is_idle_(false) {
    memset(read_buffer_, 0, sizeof(read_buffer_));
    memset(write_buffer_, 0, sizeof(write_buffer_));
    
}

TcpConnection::~TcpConnection() {
    stop();
}

void TcpConnection::start() {
	is_idle_ = false;
    do_read(); // 启动读取
}

void TcpConnection::stop() {
	is_idle_ = true;
    if (socket_.is_open()) {
        boost::system::error_code ec;
        socket_.close(ec);
    }
}

bool TcpConnection::is_idle() {
    return is_idle_.load();
}

void TcpConnection::write(const std::string& data) {
    std::lock_guard<std::mutex> lock(write_mutex_);
    write_queue_.push_back(data);
    if (write_queue_.size() == 1) {
        do_write();
    }
}

void TcpConnection::on_data_received(int result) {
	//std::cout << "PORT->TCP :" << std::this_thread::get_id() << std::endl;
    if (result > 0) {
        write(std::string(write_buffer_, result));
    }
}

DataVariant& TcpConnection::get_data() {
    return cached_data_;
}

void TcpConnection::do_read() {
    auto self(shared_from_this());
    socket_.async_read_some(
        boost::asio::buffer(read_buffer_, sizeof(read_buffer_)),
        [this, self](boost::system::error_code ec, size_t bytes_transferred) {
            handle_read(ec, bytes_transferred);
        }
    );
}

void TcpConnection::do_write() {
    auto self(shared_from_this());
    std::string data = write_queue_.front();
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(data),
        [this, self](boost::system::error_code ec, size_t bytes_transferred) {
            handle_write(ec, bytes_transferred);
        }
    );
	//std::cout << "tcp write :" << std::this_thread::get_id() << std::endl;
#ifdef DEBUG
	std::cout << "[" << std::this_thread::get_id() << "]["  << get_timestamp() << "]TCP[" << transport_id_ << "] SEND: \n";
	print_packet(reinterpret_cast<const uint8_t*>(data.c_str()),data.size());
#endif
}

void TcpConnection::handle_read(const boost::system::error_code& ec, size_t bytes_transferred) {
    if (ec) {
        logger.log(Logger::LogLevel::ERROR, "Read error({}): {}", ec.value(), ec.message());
        stop();
        return;
    }

#ifdef DEBUG
	std::cout << "[" << std::this_thread::get_id() << "]["  << get_timestamp() << "]TCP[" 
		<< transport_id_ << "][" << bytes_transferred << "] RECV: \n";
    //printf("[%s]TCP[%d] RECV[%ld]ID[%d]: \r\n", get_timestamp().c_str(), transport_id_, bytes_transferred,std::this_thread::get_id());
    print_packet(reinterpret_cast<const uint8_t*>(read_buffer_), bytes_transferred);
#endif
//std::cout << "tcp read :" << std::this_thread::get_id() << std::endl;
    if ( bytes_transferred > 0 ) {
		auto locked = transport_.lock();
		if (locked) {
			int ret = locked->input(read_buffer_, bytes_transferred, std::chrono::milliseconds(100));
			if (ret <= 0) {
				logger.log(Logger::LogLevel::WARNING, "CircularBuffer error {}, {} bytes discarded", ret, bytes_transferred);
			}
		}
    }

    do_read();
}

void TcpConnection::handle_write(const boost::system::error_code& ec, size_t bytes_transferred) {
    if (!ec) {
        std::lock_guard<std::mutex> lock(write_mutex_);
        write_queue_.pop_front();
        if (!write_queue_.empty()) {
            do_write();
        }
    } else {
        stop();
    }
}
