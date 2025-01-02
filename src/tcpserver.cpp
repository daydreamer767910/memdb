#include "tcpserver.hpp"
#include "logger.hpp"

auto& logger = Logger::get_instance();

void TcpConnection::start(uv_tcp_t* client) {
	logger.log(Logger::LogLevel::INFO,"connection starting");
	client_ = client;
	client_->data = this;
	timer.start();
	uv_read_start((uv_stream_t*)client_, on_alloc_buffer, on_read);
	ThreadBase::start();
	logger.log(Logger::LogLevel::INFO,"connection started");
}

void TcpConnection::stop() {
	timer.stop();
	ThreadBase::stop();
	//uv_close((uv_handle_t*)client_, [](uv_handle_t* handle) { delete (uv_tcp_t*)handle; });
}

// 事务处理函数
void TcpConnection::handle_transaction(uv_tcp_t* client,bool up, const char* data, ssize_t length) {
	logger.log(Logger::LogLevel::INFO,
		"transaction with data: {}",
		std::string(data, length));
	//std::this_thread::sleep_for(std::chrono::seconds(1));  // 模拟耗时操作
	if(up) { //send to uplayer
		int ret = send_data_to_app(data,length);
		if (ret != (length+4)) {
			logger.log(Logger::LogLevel::ERROR,"send_data_to_app Failed {}" , ret);
		}
		
		// 完成事务后发送响应
		uv_buf_t resbuf = uv_buf_init(const_cast<char*>("Transaction processed"), 21);
		uv_write_t* write_req = (uv_write_t*)malloc(sizeof(uv_write_t));
		ret = uv_write(write_req, (uv_stream_t*)client, &resbuf, 1, on_write);
		if (ret < 0) {
			logger.log(Logger::LogLevel::ERROR,"Failed to submit write: {}" , uv_strerror(ret));
			free(write_req);
		}
		logger.log(Logger::LogLevel::INFO,"Transaction processed");
	}
	
}

// 写入客户端的回调
void TcpConnection::on_write(uv_write_t* req, int status) {
	if (status) {
		logger.log(Logger::LogLevel::ERROR,"Write error: {}",uv_strerror(status));
	}
	free(req);  // 释放写请求
}

// 读取客户端数据的回调
void TcpConnection::on_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
	logger.log(Logger::LogLevel::INFO, "TcpConnection::on_read in");
	auto* connection = static_cast<TcpConnection*>(client->data);
	if (nread < 0) {
		logger.log(Logger::LogLevel::ERROR,"Read error({}): {}",nread,uv_strerror(nread));
		if (nread == UV_EOF) connection->stop();
	} else {
		// 将数据添加到任务队列进行后续处理
		const char* data = buf->base;  // 读取的内容
        ssize_t length = nread;        // 读取的长度
		bool toUp = true; //send to up layer
		connection->sendmsg(std::make_tuple((uv_tcp_t*)client,toUp, data, length));
		connection->keep_alive_cnt = 0;
	}
	free(buf->base);
	logger.log(Logger::LogLevel::INFO, "TcpConnection::on_read out");
}

void TcpConnection::on_msg(const Msg& msg) {
	std::visit([this](auto&& message) {
		using T = std::decay_t<decltype(message)>;
		if constexpr (std::is_same_v<T, MsgType>) {
			// 处理事务
			uv_tcp_t* client = std::get<0>(message);
			bool up = std::get<1>(message);
			const char* data = std::get<2>(message);
			ssize_t length = std::get<3>(message);

			handle_transaction(client,up, data, length);
		}
	}, msg);
}

void TcpConnection::on_timer() {
	if(keep_alive_cnt>3) {
		this->stop();
		logger.log(Logger::LogLevel::INFO, "Keep alive no resp for {} times, kick off the connect",keep_alive_cnt-1);
		keep_alive_cnt = 0;
	} 
	
	if (!this->is_idle() && keep_alive_cnt>0) {
		logger.log(Logger::LogLevel::INFO,"Processing{} keep alive {}",this->get_status(),keep_alive_cnt);
		uv_buf_t resbuf = uv_buf_init(const_cast<char*>("keep alive checking"), 20);
		uv_write_t* write_req = (uv_write_t*)malloc(sizeof(uv_write_t));
		write_req->data = this;
		int ret = uv_write(write_req, (uv_stream_t*)this->client_, &resbuf, 1, on_write);
		if (ret < 0) {
			logger.log(Logger::LogLevel::ERROR,"Failed to submit write: {}" , uv_strerror(ret));
			delete write_req;
			this->stop();
		}
	}
	keep_alive_cnt++;
}

int32_t TcpConnection::send_data_to_app(const char* data, ssize_t length) {
	// 打包数据
	uint32_t toWriteLen = length + 4;
	uint8_t header[4] = {
		static_cast<uint8_t>((length >> 24) & 0xFF),
		static_cast<uint8_t>((length >> 16) & 0xFF),
		static_cast<uint8_t>((length >> 8) & 0xFF),
		static_cast<uint8_t>(length & 0xFF)
	};

	// 写入消息到上行通道
	toWriteLen = channel_.write_up(header, sizeof(header));          // 写入长度
	toWriteLen += channel_.write_up(reinterpret_cast<const uint8_t*>(data),length); // 写入内容
	
	return toWriteLen;
}

int32_t TcpConnection::receive_data_from_app() {
	// 检查消息长度
	int32_t length = channel_.read_message_length_down();
	if (length > 0) {
		std::vector<uint8_t> buffer(length);
		// 读取完整消息
		if (channel_.read_down(buffer.data(), length)) {
			std::string message(buffer.begin(), buffer.end());
			std::cout << "[TCP] Received: " << message << std::endl;
		} else {
			std::cerr << "[TCP] Failed to read message." << std::endl;
		}
	} else {
		std::cerr << "[TCP] No message available." << std::endl;
	}
	return length;
}


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
				auto connection = std::make_shared<TcpConnection>(client);
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
	logger.log(Logger::LogLevel::INFO, "TcpServer::on_timer in: {}", connection_count );
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
	logger.log(Logger::LogLevel::INFO, "TcpServer::on_timer out: {}", connection_count );
}

