#ifndef TCPCONNECTION_HPP
#define TCPCONNECTION_HPP

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
#include "util/timer.hpp"
#include "transportsrv.hpp"


class TcpConnection {
public:
    // TcpConnection 构造函数
    TcpConnection(uv_loop_t* loop, uv_tcp_t* client, int transport_id) 
        : loop_(loop), client_(client) , transport_id_(transport_id),
        timer(60000, 60000, [this]() {
            this->on_timer();
        }){
		client_->data = this;
        keep_alive_cnt = 0;
        idle_handle_ = nullptr;
        status_ = 0;
    }

    virtual ~TcpConnection() {
        transportSrv.close_port(transport_id_);
    }

    void start(uv_tcp_t* client);
    void stop();
    bool is_idle() {
        return status_ == 0;
    }

private:
    uv_loop_t* loop_;
    uv_tcp_t* client_;  // 当前连接的 TCP 客户端
    uv_idle_t* idle_handle_;
    int status_;

    int transport_id_;
    Timer timer;//for keep alive

    int keep_alive_cnt;
    char read_buf[512];
    char write_buf[512];

    //timer for keep alive
    void on_timer();

    void on_pull();
	
    static void process(uv_idle_t* handle);

    int32_t write(const char* data,ssize_t length);
    // 写入客户端的回调
    static void on_write(uv_write_t* req, int status);
	static void on_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf);
	// 分配内存的回调
    static void on_alloc_buffer(uv_handle_t* handle, size_t /*suggested_size*/, uv_buf_t* buf) {
        buf->base = reinterpret_cast<TcpConnection*>(handle->data)->read_buf;
        buf->len = sizeof(reinterpret_cast<TcpConnection*>(handle->data)->read_buf);
    }

};

#endif