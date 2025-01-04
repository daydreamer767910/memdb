#include "tcpconnection.hpp"
#include "log/logger.hpp"


void TcpConnection::start(uv_tcp_t* client) {
	client_ = client;
	client_->data = this;
	timer.start();
	auto channel_ptr = transportSrv.get_transport(transport_id_);
	if (channel_ptr) channel_ptr->resetBuffers();
	uv_read_start((uv_stream_t*)client_, on_alloc_buffer, on_read);
	ThreadBase::start();
	logger.log(Logger::LogLevel::INFO,"connection started");
}

void TcpConnection::stop() {
	timer.stop();
	ThreadBase::stop();
	//uv_close((uv_handle_t*)client_, [](uv_handle_t* handle) { delete (uv_tcp_t*)handle; });
}

// 写入客户端
int32_t TcpConnection::write(const char* data,ssize_t length) {
	uv_write_t* write_req = (uv_write_t*)malloc(sizeof(uv_write_t));
	uv_buf_t buf = uv_buf_init(const_cast<char*>(data), length);
	int32_t ret = uv_write(write_req, (uv_stream_t*)client_, &buf, 1, on_write);
	if (ret < 0) {
		logger.log(Logger::LogLevel::ERROR,"write err {}: {}" ,ret, uv_strerror(ret));
		free(write_req);
		stop();
	}
	return ret;
}

// 写入客户端的回调
void TcpConnection::on_write(uv_write_t* req, int status) {
	if (status) {
		logger.log(Logger::LogLevel::ERROR,"Write error {}: {}",status,uv_strerror(status));
	}
	free(req);  // 释放写请求
	if (status == UV_EPIPE) {
		logger.log(Logger::LogLevel::ERROR,"Connection closed by peer.");
	}
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
		std::vector<char> buffer(nread);
		std::memcpy(buffer.data() , buf->base, nread);
        int length = nread;        // 读取的长度
		int msg_type = 1; //send to up layer
		connection->sendmsg(std::make_shared<ThreadMsg>(std::make_tuple(msg_type, length, buffer)));
		
		connection->write("ack",3);
		connection->keep_alive_cnt = 0;
	}
	logger.log(Logger::LogLevel::INFO, "TcpConnection::on_read out");
}

void TcpConnection::on_msg(const std::shared_ptr<ThreadMsg> msg) {
	std::visit([this](auto&& message) {
		using T = std::decay_t<decltype(message)>;
		if constexpr (std::is_same_v<T, MsgType>) {
			// 处理事务,
			int msg_type = std::get<0>(message);
			int length = std::get<1>(message);
			std::vector<char> buffer = std::get<2>(message);

			std::cout << "on_msg:" << length << " [" << std::string(buffer.begin(), buffer.end()) << "]\n";
			auto channel_ptr = transportSrv.get_transport(transport_id_);
			if (msg_type == 1) { //to app
				// Write socket data to CircularBuffer
				if (channel_ptr && !channel_ptr->tcpReceive(buffer.data(), length, std::chrono::milliseconds(1000))) {
					logger.log(Logger::LogLevel::WARNING, "CircularBuffer full, data discarded");
				}
				//notify app layer, tbd
			} else { //to socket
				while(length > 0) {
					size_t len = std::min(static_cast<size_t>(length), sizeof(write_buf));
					//read from circular buffer
					if (channel_ptr && channel_ptr->tcpReadFromApp(write_buf,len,std::chrono::milliseconds(1000))) {
						//send to client socket
						write(write_buf,len);
						length -= len;
					} else {
						logger.log(Logger::LogLevel::WARNING, "[TCP] No message available in CircularBuffer");
						break; // Avoid infinite loop
					}
				}
			}
		}
	}, *msg);
}

void TcpConnection::on_timer() {
	if(keep_alive_cnt>3) {
		this->stop();
		logger.log(Logger::LogLevel::INFO, "Keep alive no resp for {} times, kick off the connect",keep_alive_cnt-1);
		keep_alive_cnt = 0;
	} 
	
	if (!this->is_idle() && keep_alive_cnt>0) {
		logger.log(Logger::LogLevel::INFO,"Processing{} keep alive {}",this->get_status(),keep_alive_cnt);
		write("keep alive checking", 20);
	}
	keep_alive_cnt++;
}
