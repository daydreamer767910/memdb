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

#define TCP_KEEPALIVE_TIMER 60000 //ms
#define TCP_BUFFER_SIZE 512
class TcpConnection {
public:
    // TcpConnection 构造函数
    TcpConnection(uv_loop_t* loop, uv_tcp_t* client) 
        : loop_(loop), client_(client) , transport_id_(0),
        timer(loop,TCP_KEEPALIVE_TIMER, TCP_KEEPALIVE_TIMER, [this]() {
            this->on_timer();
        }){
		client_->data = this;
        keep_alive_cnt = 0;
        status_ = 0;
    }

    virtual ~TcpConnection() {
        
    }

    void start(uv_tcp_t* client, int transport_id);
    void stop();
    bool is_idle() {
        return status_ == 0;
    }
    void on_poll(uv_poll_t* handle);
    
private:
    uv_loop_t* loop_;
    uv_tcp_t* client_;  // 当前连接的 TCP 客户端
    int status_;
    int transport_id_;
    Timer timer;//for keep alive

    int keep_alive_cnt;
    char read_buf[TCP_BUFFER_SIZE];
    char write_buf[TCP_BUFFER_SIZE];

    //timer for keep alive
    void on_timer();

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