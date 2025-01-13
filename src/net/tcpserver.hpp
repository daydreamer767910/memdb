#ifndef TCPSERVER_HPP
#define TCPSERVER_HPP

#include <boost/asio.hpp>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <mutex>
#include "tcpconnection.hpp"
#include "log/logger.hpp"
#include "transport.hpp"

#define MAX_CONNECTIONS 1000

class TcpServer {
public:
    TcpServer(const std::string& ip, int port);
    void start();
    void stop();

private:
    void on_new_connection(const boost::system::error_code& error, boost::asio::ip::tcp::socket socket);
    void cleanup();
    void on_timer();

    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::unordered_map<uint32_t, std::shared_ptr<TcpConnection>> connections_;
    std::mutex mutex_;
    std::atomic<int> connection_count;
    std::atomic<uint32_t> unique_id;
    uint32_t max_connection_num = MAX_CONNECTIONS;
    std::shared_ptr<boost::asio::steady_timer> timer_;
};

#endif // TCPSERVER_HPP
