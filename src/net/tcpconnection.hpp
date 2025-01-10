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

#define TCP_BUFFER_SIZE 1460

class TcpConnection :public IDataCallback {
    friend class TcpServer;
public:
    // TcpConnection 构造函数
    TcpConnection(uv_loop_t* loop, uv_tcp_t* client,trans_pair& tranport_info) 
        : loop_(loop), client_(client) {
		client_->data = this;
        status_ = -1;
        transport_id_ = tranport_info.first;
        transport_ = tranport_info.second;
    }

    virtual ~TcpConnection() {
#ifdef DEBUG
        std::cout << "tcp client[" << client_ip << ":" << client_port << "] destoryed" << std::endl;
#endif
    }

    void start();
    void stop();
    bool is_idle() {
        return status_ == 0;
    }
    
    DataVariant& get_data() override {
		memset(write_buf,0,sizeof(write_buf));
		cached_data_ = std::make_tuple(write_buf, sizeof(write_buf), transport_id_);
        return cached_data_;
    }
	void on_data_received(int result) override ;
private:
    uv_loop_t* loop_;
    uv_tcp_t* client_;  // 当前连接的 TCP 客户端
    int status_;
    uint32_t transport_id_;
    std::shared_ptr<Transport> transport_;
    char client_ip[INET6_ADDRSTRLEN];
	int client_port;

    char read_buf[TCP_BUFFER_SIZE];
    char write_buf[TCP_BUFFER_SIZE];
    DataVariant cached_data_;

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