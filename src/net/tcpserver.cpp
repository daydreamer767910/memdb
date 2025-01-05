#include "tcpserver.hpp"
#include "transportsrv.hpp"
#include "log/logger.hpp"


// 初始化连接计数
std::atomic<int> TcpServer::connection_count(0);

TcpServer::TcpServer(const char* ip, int port):	timer(10000, 10000, [this]() {
        this->on_timer();
    }) {
	loop_ = uv_default_loop();
    uv_ip4_addr(ip, port, &addr);
    uv_tcp_init(loop_, &server);
    uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
	server.data = this;
	
}

TcpServer::~TcpServer() {
	//timer.stop();
	// 遍历并清理所有连接
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [tcp, connection] : connections_) {
            // 关闭 uv_tcp_t，并在回调中释放内存
            uv_close((uv_handle_t*)tcp, [](uv_handle_t* handle) {
                delete (uv_tcp_t*)handle;
            });
        }
        connections_.clear(); // 清空 map
    }
    uv_close((uv_handle_t*)&server, nullptr);
}

// 启动服务器
void TcpServer::start() {
	int r = uv_listen((uv_stream_t*)&server, 128, on_new_connection);
	if (r) {
		std::cerr << "Listen error: " << uv_strerror(r) << std::endl;
		return;
	}
	logger.log(Logger::LogLevel::INFO,"TCP server listening on 0.0.0.0:{}",ntohs(addr.sin_port));
	//std::cout << "TCP server listening on " << "0.0.0.0" << ":" << ntohs(addr.sin_port) << std::endl;
	ThreadBase::start();
	//timer.start();
	// Run the event loop
    uv_run(loop_, UV_RUN_DEFAULT);
}

// 处理新连接
void TcpServer::on_new_connection(uv_stream_t* server, int status) {
    if (status < 0) {
        std::cerr << "New connection error: " << uv_strerror(status) << std::endl;
        return;
    }
	logger.log(Logger::LogLevel::INFO, "TcpServer::on_new_connection in");
	auto* tcp_server = static_cast<TcpServer*>(server->data);
    uv_tcp_t* client = new uv_tcp_t;
    uv_tcp_init(tcp_server->loop_, client);
	{
		std::lock_guard<std::mutex> lock(tcp_server->mutex_);

		if (uv_accept(server, (uv_stream_t*)client) == 0 && connection_count < 50) {
			auto it = std::find_if(tcp_server->connections_.begin(), tcp_server->connections_.end(),
							[](const std::pair<uv_tcp_t*, std::shared_ptr<TcpConnection>>& pair) {
									return pair.second->is_idle(); });
			if (it != tcp_server->connections_.end()) {
				uv_close((uv_handle_t*)it->first, [](uv_handle_t* handle) { delete (uv_tcp_t*)handle; });
				tcp_server->connections_[client] = it->second; //更新map,增加新的k,旧的v
				tcp_server->connections_.erase(it); //更新map,删除旧的k,v
				it->second->start(client);  // 使用已停止的客户端实例，重新启动
				logger.log(Logger::LogLevel::INFO,"Reusing stopped connection for new client.");
			} else {
				connection_count++;
				int port_id = transportSrv.open_new_port(4096);
				auto connection = std::make_shared<TcpConnection>(client,port_id);
				tcp_server->connections_.emplace(client, connection);
				connection->start(client);
				logger.log(Logger::LogLevel::INFO,"Created a new client for new connection.");
			}
		} else {
			uv_close((uv_handle_t*)client, nullptr);
			delete client;
		}
	}
	logger.log(Logger::LogLevel::INFO, "TcpServer::on_new_connection out");
}

// 处理事务
void TcpServer::on_timer() {
	//logger.log(Logger::LogLevel::INFO, "TcpServer::on_timer in: {}", connection_count );
	std::lock_guard<std::mutex> lock(mutex_);
	while (connection_count >= 10) { //连接数太多需要清理停止的connection
		auto it = std::find_if(connections_.begin(), connections_.end(),
                           [](const auto& pair) { return pair.second->is_idle(); });
		if (it != connections_.end()) {
			connection_count--;
			uv_close((uv_handle_t*)it->first, [](uv_handle_t* handle) { delete (uv_tcp_t*)handle; });
			connections_.erase(it);
			logger.log(Logger::LogLevel::INFO, "An idle connection erased {} connections remain", connection_count );
		} else {
			break;
		}
	} 
	//logger.log(Logger::LogLevel::INFO, "TcpServer::on_timer out: {}", connection_count );
}

void TcpServer::on_msg(const std::shared_ptr<ThreadMsg> msg) {
	std::visit([this](auto&& message) {
		using T = std::decay_t<decltype(message)>;
		if constexpr (std::is_same_v<T, MsgType>) {
			// 处理事务,
			int msg_type = std::get<0>(message);
			int length = std::get<1>(message);
			std::vector<char> buffer = std::get<2>(message);

			
		}
	}, *msg);
}
