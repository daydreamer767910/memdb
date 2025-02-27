#ifndef TCPSERVER_HPP
#define TCPSERVER_HPP

#include <boost/asio.hpp>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <mutex>
#include "tcpconnection.hpp"
#include "log/logger.hpp"
#include "util/timer.hpp"

#define MAX_CONNECTIONS 1000

class TcpServer {
public:
    TcpServer();
    ~TcpServer() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            #ifdef DEBUG
            std::cout << "TcpServer destroyed!" << std::endl;
            #endif
            connections_.clear();
        }
    }
    void start(const std::string& ip, int port);
    void stop();
    void add_observer(const std::shared_ptr<IConnectionObserver>& observer) {
        observers_.push_back(observer);
    }

private:
    void on_new_connection(const boost::system::error_code& error, boost::asio::ip::tcp::socket socket);
    void cleanup();
    void on_timer(std::thread::id id);
    void notify_new_connection(const std::shared_ptr<IConnection>& connection) {
        for (const auto& observer : observers_) {
            observer->on_new_connection(connection);
        }
    }
	void notify_close_connection(const uint32_t port_id) {
        for (const auto& observer : observers_) {
            observer->on_close_connection(port_id);
        }
    }

    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::unordered_map<uint32_t, std::shared_ptr<TcpConnection>> connections_;
    std::mutex mutex_;
    std::atomic<int> connection_count;
    std::atomic<uint32_t> unique_id;
    uint32_t max_connection_num = MAX_CONNECTIONS;
    Timer timer_;
    std::vector<std::shared_ptr<IConnectionObserver>> observers_;
};

#endif // TCPSERVER_HPP
