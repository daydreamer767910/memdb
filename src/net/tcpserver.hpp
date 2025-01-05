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
#include "util/timer.hpp"
#include "util/threadbase.hpp"
#include "transport.hpp"
#include "tcpconnection.hpp"

using MsgType = std::tuple<int , int ,std::vector<char>>;
using ThreadMsg = std::variant<MsgType>;

// TCP 服务器类
class TcpServer : public ThreadBase<MsgType>{
public:
    TcpServer(const char* ip, int port);
	~TcpServer();
    // 启动服务器
    void start() override;

    // 处理新连接
    static void on_new_connection(uv_stream_t* server, int status);

    // 处理事务（在新线程中运行）
    void on_timer();

    void on_msg(const std::shared_ptr<ThreadMsg> msg) override;
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
