#ifndef TCPSERVER_HPP
#define TCPSERVER_HPP

#include <iostream>
#include <uv.h>
#include <fstream>
#include <string>
#include <cstring>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <tuple>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include "timer.hpp"
#include "threadbase.hpp"
#include "bichannel.hpp"

using MsgType = std::tuple<uv_tcp_t*,bool , const char*, ssize_t>;
using Msg = std::variant<MsgType>;
class TcpConnection: public ThreadBase<MsgType> {
public:
    // TcpConnection 构造函数
    TcpConnection(uv_tcp_t* client) 
        : client_(client) , channel_(1024),
        timer(10000, 30000, [this]() {
            this->on_timer();
        }){
		client_->data = this;
        keep_alive_cnt = 0;
    }
    void start(uv_tcp_t* client);
    void stop() override;
private:
    DoubleChannel channel_;
    uv_tcp_t* client_;  // 当前连接的 TCP 客户端
    Timer timer;//for keep alive
    int keep_alive_cnt;


    void on_timer();
	void on_msg(const Msg& msg) override;
    // 事务处理函数
    void handle_transaction(uv_tcp_t* client, bool up, const char* data, ssize_t length);
    // 写入客户端的回调
    static void on_write(uv_write_t* req, int status);
	static void on_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf);
	// 分配内存的回调
    static void on_alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
        buf->base = (char*)malloc(suggested_size);
        buf->len = suggested_size;
    }

    int32_t send_data_to_app(const char* data, ssize_t length);
    int32_t receive_data_from_app();
};

// TCP 服务器类
class TcpServer : public ThreadBase<int>{
public:
    TcpServer(const char* ip, int port);
	~TcpServer();
    // 启动服务器
    void start() override;

    // 处理新连接
    static void on_new_connection(uv_stream_t* server, int status);

    // 处理事务（在新线程中运行）
    void on_timer();
private:
    uv_loop_t* loop_;
    uv_tcp_t server;
    struct sockaddr_in addr;
    Timer timer;//定期清理无用connection
    std::mutex mutex_;  // 互斥锁，保护 connections_ 容器
    static std::atomic<int> connection_count;  // 连接数
	std::unordered_map<uv_tcp_t*, std::shared_ptr<TcpConnection>> connections_;
};

#endif
