#include "tcpconnection.hpp"
#include "log/logger.hpp"
#include "server/dbservice.hpp"
#include "util/util.hpp"

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
	printf("TCP[%d] SEND: \r\n", transport_id_);
	print_packet(reinterpret_cast<const uint8_t*>(data),length);
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
	//logger.log(Logger::LogLevel::INFO, "TcpConnection::on_read in");
	auto* connection = static_cast<TcpConnection*>(client->data);
	if (nread < 0) {
		logger.log(Logger::LogLevel::ERROR,"Read error({}): {}",nread,uv_strerror(nread));
		if (nread == UV_EOF) connection->stop();
	} else {
		auto channel_ptr = transportSrv.get_transport(connection->transport_id_);
		if (!channel_ptr) {
			logger.log(Logger::LogLevel::WARNING, "transport is unavailable for TCP recv");
			connection->write("internal err",12);
		}
		else if (channel_ptr->tcpReceive(buf->base, nread, std::chrono::milliseconds(1000)) < 0) {
			logger.log(Logger::LogLevel::WARNING, "CircularBuffer full, data discarded");
			connection->write("buffer full, data discarded",27);
		} else {
			connection->write("ack",3);
		}
		connection->keep_alive_cnt = 0;
	}
	//logger.log(Logger::LogLevel::INFO, "TcpConnection::on_read out");
}

void TcpConnection::process() {
	while (true) {
		auto channel_ptr = transportSrv.get_transport(transport_id_);
		if (!channel_ptr) {
			logger.log(Logger::LogLevel::ERROR, "Port[{}] is unavailable",transport_id_);
			this->stop();
			break;
		}
		int len = sizeof(write_buf);
		//read from circular buffer
		len = channel_ptr->tcpReadFromApp(write_buf,len,std::chrono::milliseconds(1000));
		//std::cout << "TcpConnection::process:" << len << std::endl;
		if (len>0) {
			//send to client socket
			write(write_buf,len);
		}
	}
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
