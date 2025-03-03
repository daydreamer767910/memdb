#include "tcpconnection.hpp"
#include "log/logger.hpp"
#include "util/util.hpp"
#include "transport.hpp"

TcpConnection::TcpConnection(boost::asio::io_context& io_context, tcp::socket socket, uint32_t id)
    : io_context_(io_context), 
      socket_(std::move(socket)),
      is_idle_(false),
      id_(id) {
    memset(read_buffer_, 0, sizeof(read_buffer_));
    memset(write_buffer_, 0, sizeof(write_buffer_));
    cached_data_ = std::make_tuple(write_buffer_, sizeof(write_buffer_), id_);
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
    try {
        if (!socket_.is_open()) return;  // 避免重复关闭

        boost::system::error_code ec;
        socket_.cancel(ec);  // 确保异步操作终止
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);

        if (ec) {
            std::cerr << "Error closing socket: " << ec.message() << std::endl;
        } else {
            #ifdef DEBUG
            std::cout << "Connection closed." << std::endl;
            #endif
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception during close: " << e.what() << std::endl;
    }
}

bool TcpConnection::is_idle() {
    return is_idle_.load();
}

void TcpConnection::on_data_received(int len, int ) {
	//std::cout << std::dec << get_timestamp() << " : PORT->TCP :" << std::this_thread::get_id() << std::endl;
    if (len > 0) {
        //write(std::string(write_buffer_, len));
        auto self(shared_from_this());
        boost::asio::async_write(
            socket_,
            boost::asio::buffer(write_buffer_, len),
            [this, self](boost::system::error_code ec, size_t bytes_transferred) {
                if (!ec) {
                #ifdef DEBUG
                    std::cout << std::dec << "PID[" << std::this_thread::get_id() << "]["  << get_timestamp() 
                        << "]TCP[" << id_ << "] SEND[" << bytes_transferred << "]: \n";
                    print_packet(reinterpret_cast<const uint8_t*>(write_buffer_),bytes_transferred);
                #endif
                } else {
                    std::cerr << "Error on send: " << ec.message() << std::endl;
                    stop();
                }
            }
        );
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

void TcpConnection::handle_read(const boost::system::error_code& error, std::size_t nread) {
	#ifdef DEBUG
	if(nread>0) {
		std::cout << std::dec << "PID[" << std::this_thread::get_id() << "]["  << get_timestamp() << "]TCP[" 
		<< id_ << "]RECV[" << nread << "]:\n";
		print_packet(reinterpret_cast<const uint8_t*>(read_buffer_),nread);
	}
	#endif
	if (!error && nread>0) {
		auto port = transport_.lock();
		if (port) {
			int ret = port->input(read_buffer_, nread,std::chrono::milliseconds(100));
			if (ret <= 0) {
				std::cerr << "write CircularBuffer err, data discarded!" << std::endl;
			}
		} else {
			std::cerr << "transport is unavialable, data discarded!" << std::endl;
		}
	} else {
		if (error == boost::asio::error::operation_aborted
			|| error == boost::asio::error::not_connected || error ==boost::asio::error::bad_descriptor){
			#ifdef DEBUG
			std::cout << "Socket is closed locally." << std::endl;
			#endif
			
		} else {
			std::cerr << "Error on receive: " << error.message() << std::endl;
			stop();
		}
		return;
	}
	// 仅在连接成功后设置异步读取
    if (socket_.is_open()) {
        do_read();
    } else {
		#ifdef DEBUG
        std::cerr << "Socket is closed, cannot set async read." << std::endl;
		#endif
    }
}

