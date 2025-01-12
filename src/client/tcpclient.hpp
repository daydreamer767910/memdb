#ifndef TCPCLIENT_HPP
#define TCPCLIENT_HPP

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

#define TCP_BUFFER_SIZE 1460

class TcpClient {
protected:
	virtual void handle_read(const boost::system::error_code& error, size_t nread) = 0;
	void set_async_read(char* buf, size_t buf_size) {
		socket_.async_read_some(
			boost::asio::buffer(buf, buf_size),
			[this](const boost::system::error_code& error, std::size_t nread) {
				this->handle_read(error, nread);
			}
		);
	}
public:
    TcpClient(boost::asio::io_context& io_context)
        : io_context_(io_context), socket_(io_context), timer_(io_context) {}

    // 连接接口
    bool connect(const std::string& host, const std::string& port) {
        try {
            boost::asio::ip::tcp::resolver resolver(io_context_);
            boost::asio::connect(socket_, resolver.resolve(host, port));
            std::cout << "Connected to " << host << ":" << port << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error connecting to server: " << e.what() << std::endl;
            return false;
        }
    }

	bool is_connected() const {
		return socket_.is_open();
	}

    // 读接口，支持超时
    int read_with_timeout(char* buffer, size_t length, int timeout_ms) {
		boost::system::error_code ec;
		size_t bytes_read = 0;

		try {
			// 设置超时
			timer_.expires_after(std::chrono::milliseconds(timeout_ms));
			bool timeout_occurred = false;

			// 异步等待超时
			timer_.async_wait([&](const boost::system::error_code& timer_ec) {
				if (!timer_ec) {
					timeout_occurred = true;
					socket_.cancel();  // 超时后取消读操作
				}
			});

			// 同步读取数据
			bytes_read = socket_.read_some(boost::asio::buffer(buffer, length), ec);

			// 取消超时定时器
			timer_.cancel();

			if (timeout_occurred) {
				std::cerr << "Read timeout occurred" << std::endl;
				return 0;  // 超时，未读取数据
			}

			if (ec) {
				std::cerr << "Read error: " << ec.message() << " (code: " << ec.value() << ")" << std::endl;
				if (ec == boost::asio::error::eof || ec == boost::asio::error::connection_reset
					|| ec == boost::asio::error::not_connected || ec ==boost::asio::error::bad_descriptor){
						socket_.close();
						return -2;
				}
				return -1;  // 出现错误
			}

			//std::cout << "Bytes read: " << bytes_read << std::endl;
			return static_cast<int>(bytes_read);  // 返回读取的字节数
		} catch (const std::exception& e) {
			std::cerr << "Exception during read: " << e.what() << std::endl;
			return -1;
		}
	}



    // 写接口，支持超时
    int write_with_timeout(const char* buffer, size_t length, int timeout_ms) {
		boost::system::error_code ec;
		size_t bytes_written = 0;
		bool timeout_occurred = false;

		try {
			// 设置超时
			timer_.expires_after(std::chrono::milliseconds(timeout_ms));
			timer_.async_wait([&](const boost::system::error_code& timer_ec) {
				if (!timer_ec) {
					timeout_occurred = true;
					socket_.cancel();  // 超时后取消写操作
				}
			});

			// 同步写操作
			bytes_written = boost::asio::write(socket_, boost::asio::buffer(buffer, length), ec);

			// 取消定时器
			timer_.cancel();

			if (timeout_occurred) {
				std::cerr << "Write timeout occurred" << std::endl;
				return 0;  // 返回0表示超时
			}

			if (ec) {
				std::cerr << "Write error: " << ec.message() << " (code: " << ec.value() << ")" << std::endl;
				if (ec == boost::asio::error::broken_pipe|| ec == boost::asio::error::connection_reset
					|| ec == boost::asio::error::not_connected || ec ==boost::asio::error::bad_descriptor) {
						socket_.close();
						return -2;
				}
					
				return -1;  // 返回-1表示写入失败
			}

			return static_cast<int>(bytes_written);  // 返回写入的字节数
		} catch (const std::exception& e) {
			std::cerr << "Exception during write: " << e.what() << std::endl;
			return -1;
		}
	}


    // 关闭接口
    void close() {
        try {
			socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
			socket_.close();
			std::cout << "Connection closed." << std::endl;
		} catch (const std::exception& e) {
			std::cerr << "Exception during lose: " << e.what() << std::endl;
		}
    }

private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::steady_timer timer_;
};

#endif