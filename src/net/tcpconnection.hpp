#ifndef TCPCONNECTION_HPP
#define TCPCONNECTION_HPP

#include <boost/asio.hpp>
#include <memory>
#include <atomic>
#include <iostream>
#include <deque>
#include "common.hpp"

using tcp = boost::asio::ip::tcp; // 简化命名空间


class Transport;
class TcpConnection : public IDataCallback, public Subject<TcpConnection>, public std::enable_shared_from_this<TcpConnection> {
public:
    TcpConnection(boost::asio::io_context& io_context, tcp::socket socket, uint32_t id);
    ~TcpConnection();
    
    void start();    // 启动连接
    void stop();     // 停止连接
    //bool is_idle();  // 判断是否空闲
    //void write(const std::string& data); // 主动发送数据


    void on_data_received(int len, int ) override;
    DataVariant& get_data() override;
    void set_transport(const std::shared_ptr<Transport>& transport) {
        transport_ = transport;
    }
    uint32_t get_id(){
        return id_;
    }
private:
    void do_read();          // 异步读取数据
    //void do_write();         // 异步写入数据
    void handle_read(const boost::system::error_code& ec, size_t bytes_transferred);
    
    //void handle_write(const boost::system::error_code& ec, size_t bytes_transferred);

    boost::asio::io_context& io_context_;
    tcp::socket socket_;
    //std::atomic<bool> is_idle_;
    uint32_t id_;
    std::weak_ptr<Transport> transport_;
    
    char read_buffer_[TCP_BUFFER_SIZE];     // 读缓冲区
    char write_buffer_[TCP_BUFFER_SIZE];    // 写缓冲区
    DataVariant cached_data_;    // 数据缓存
};
#endif // TCPCONNECTION_HPP