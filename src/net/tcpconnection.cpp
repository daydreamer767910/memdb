#include "tcpconnection.hpp"
#include "log/logger.hpp"
#include "util/util.hpp"

void TcpConnection::start(uv_tcp_t* client, int transport_id) {
	client_ = client;
	client_->data = this;
	transport_id_ = transport_id;

	uv_read_start((uv_stream_t*)client_, on_alloc_buffer, on_read);

	status_ = 1;
	//logger.log(Logger::LogLevel::INFO,"connection started");
}

void TcpConnection::stop() {
	status_ = 0;
	if (client_) {
		uv_close((uv_handle_t*)client_, [](uv_handle_t* handle) { delete (uv_tcp_t*)handle; });
	}
	TransportSrv::get_instance().close_port(transport_id_);
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
	//printf("[%s]TCP[%d] SEND: \r\n", get_timestamp().c_str(), transport_id_);
	//print_packet(reinterpret_cast<const uint8_t*>(data),length);
	return ret;
}

// 写入客户端的回调
void TcpConnection::on_write(uv_write_t* req, int status) {
	//printf("on_write[%d]\n",status);
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
	auto connection = static_cast<TcpConnection*>(client->data);
	if (nread < 0) {
		logger.log(Logger::LogLevel::ERROR,"Read error({}): {}",nread,uv_strerror(nread));
		if (nread == UV_EOF) {
			printf("Read error(%d): {%s}\n",nread,uv_strerror(nread));
			connection->stop();
		}
	} else {
		//printf("[%s]TCP[%d] RECV[%d]: \r\n", get_timestamp().c_str(), connection->transport_id_,nread);
		//print_packet(reinterpret_cast<const uint8_t*>(buf->base),nread);
		auto port = TransportSrv::get_instance().get_port(connection->transport_id_);
		if (port) {
			int ret = port->input(buf->base, nread,std::chrono::milliseconds(100));
			if (ret == 0) {
				logger.log(Logger::LogLevel::ERROR, "transport is unavailable for TCP recv");
			}
			else if (ret < 0) {
				logger.log(Logger::LogLevel::WARNING, "CircularBuffer full, data discarded");
			}
		} 		
	}
	//logger.log(Logger::LogLevel::INFO, "TcpConnection::on_read out");
}



