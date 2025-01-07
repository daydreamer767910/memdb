#include "tcpserver.hpp"
#include "transportsrv.hpp"
#include "log/logger.hpp"


// 初始化连接计数
std::atomic<int> TcpServer::connection_count(0);

TcpServer::TcpServer(const char* ip, int port):	loop_(uv_default_loop()),
	timer(loop_, 1000, 1000, [this]() {
        this->on_timer();
    }) {
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
			connection->stop();
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
	
	// Run the event loop
    uv_run(loop_, UV_RUN_DEFAULT);
}

void TcpServer::stop() {
	uv_stop(loop_);
}
// 处理新连接
void TcpServer::on_new_connection(uv_stream_t* server, int status) {
    if (status < 0) {
        std::cerr << "New connection error: " << uv_strerror(status) << std::endl;
        return;
    }
	//logger.log(Logger::LogLevel::INFO, "TcpServer::on_new_connection in");
	auto* tcp_server = static_cast<TcpServer*>(server->data);
    uv_tcp_t* client = new uv_tcp_t;
    uv_tcp_init(tcp_server->loop_, client);
	{
		std::lock_guard<std::mutex> lock(tcp_server->mutex_);

		if (uv_accept(server, (uv_stream_t*)client) == 0 ) {
			struct sockaddr_storage client_addr;
			int addr_len = sizeof(client_addr);
			char client_ip[INET6_ADDRSTRLEN] = {0};
			int client_port = 0;
			if (uv_tcp_getpeername(client, (struct sockaddr*)&client_addr, &addr_len) == 0) {
				if (client_addr.ss_family == AF_INET) {
					struct sockaddr_in* addr = (struct sockaddr_in*)&client_addr;
					uv_ip4_name(addr, client_ip, sizeof(client_ip));
					client_port = ntohs(addr->sin_port);
				} else if (client_addr.ss_family == AF_INET6) {
					struct sockaddr_in6* addr = (struct sockaddr_in6*)&client_addr;
					uv_ip6_name(addr, client_ip, sizeof(client_ip));
					client_port = ntohs(addr->sin6_port);
				}
			} else {
				fprintf(stderr, "Failed to get client address\n");
			}
			if (connection_count < TcpServer::max_connection_num) {
				connection_count++;
				int port_id = TransportSrv::get_instance().open_new_port(TcpServer::transport_buff_szie);
				auto connection = std::make_shared<TcpConnection>(tcp_server->loop_, client,port_id);
				tcp_server->connections_.emplace(client, connection);
				connection->start(client, port_id);
				logger.log(Logger::LogLevel::INFO,"New connection[{}:{}]:transport[{}]",
					client_ip, client_port, port_id);
			} else {
				uv_close((uv_handle_t*)client, nullptr);
				delete client;
			}
		} else {
			uv_close((uv_handle_t*)client, nullptr);
			delete client;
		}
	}
}

// 处理事务
void TcpServer::on_timer() {
	std::lock_guard<std::mutex> lock(mutex_);
	//logger.log(Logger::LogLevel::INFO, "TcpServer::on_timer : {}", connection_count );
	while (true) { //IDLE连接数太多需要清理IDLE的connection
		auto it = std::find_if(connections_.begin(), connections_.end(),
                           [](const auto& pair) { return pair.second->is_idle() ; });
		if (it != connections_.end()) {
			connection_count--;
			connections_.erase(it);
			logger.log(Logger::LogLevel::INFO, "Idle erased, {} connections remain", connection_count );
		} else {
			break;
		}
	}
}

