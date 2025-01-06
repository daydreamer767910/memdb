#include "tcpconnection.hpp"
#include "log/logger.hpp"
#include "server/dbservice.hpp"
#include "util/util.hpp"

void TcpConnection::start(uv_tcp_t* client) {
	client_ = client;
	client_->data = this;
	keep_alive_cnt = 0;
	timer.start();
	auto channel_ptr = transportSrv.get_transport(transport_id_);
	if (channel_ptr) channel_ptr->resetBuffers();
	uv_read_start((uv_stream_t*)client_, on_alloc_buffer, on_read);

	idle_handle_ = (uv_idle_t*)malloc(sizeof(uv_idle_t));
	idle_handle_->data = this;
    uv_idle_init(loop_, idle_handle_);
    uv_idle_start(idle_handle_, process);
	status_ = 1;

	logger.log(Logger::LogLevel::INFO,"connection started");
}

void TcpConnection::stop() {
	status_ = 0;
	keep_alive_cnt = 0;
	timer.stop();
	uv_idle_stop(idle_handle_);  // 停止轮询
    free(idle_handle_);  // 释放内存
}

// 写入客户端
int32_t TcpConnection::write(const char* data,ssize_t length) {
	if (uv_is_closing((uv_handle_t*)client_)) {
        std::cerr << "Stream is closing, cannot write.\n";
        return -1;
    }
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
	printf("on_write[%d]\n",status);
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

void TcpConnection::on_pull() {
	auto channel_ptr = transportSrv.get_transport(transport_id_);
	if (!channel_ptr) {
		logger.log(Logger::LogLevel::ERROR, "Port[{}] is unavailable",transport_id_);
		stop();
		return;
	}
	int len = sizeof(write_buf);
	//read from circular buffer
	len = channel_ptr->tcpReadFromApp(write_buf,len,std::chrono::milliseconds(50));
	//std::cout << "TcpConnection::process:" << len << std::endl;
	if (len>0) {
		//send to client socket
		write(write_buf,len);
		keep_alive_cnt = 0;
	}
}

void TcpConnection::process(uv_idle_t* handle) {
	TcpConnection* connection_ptr = static_cast<TcpConnection*>(handle->data);
	if (connection_ptr) connection_ptr->on_pull();
}

void TcpConnection::on_timer() {
	if(keep_alive_cnt>2) {
		this->stop();
		logger.log(Logger::LogLevel::INFO, "Keep alive no resp for {} times, kick off the connect",keep_alive_cnt-1);
		
	} else if ( keep_alive_cnt>0) {
		logger.log(Logger::LogLevel::INFO,"Processing keep alive {}",keep_alive_cnt);
		write("keep alive checking", 20);
	}
	keep_alive_cnt++;

}
