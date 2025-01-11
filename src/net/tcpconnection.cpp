#include "tcpconnection.hpp"
#include "log/logger.hpp"
#include "util/util.hpp"

void TcpConnection::start() {
	uv_read_start((uv_stream_t*)client_, on_alloc_buffer, on_read);

	status_ = 1;
	//logger.log(Logger::LogLevel::INFO,"connection started");
}

void TcpConnection::stop() {
	status_ = 0;
	if (client_) {
		uv_close((uv_handle_t*)client_, [](uv_handle_t* handle) { delete (uv_tcp_t*)handle; });
	}
}

//提供给传输层的回调
void TcpConnection::on_data_received(int result) {
	if (result > 0)
		write(write_buf,result);
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
		if (ret == UV_ECONNRESET || ret == UV_EPIPE) {
			std::cerr << "write err " << uv_strerror(ret) << " and close the connection" << std::endl;
			stop();
		}
	}
#ifdef DEBUG
	printf("[%s]TCP[%d] SEND: \r\n", get_timestamp().c_str(), transport_id_);
	print_packet(reinterpret_cast<const uint8_t*>(data),length);
#endif
	return ret;
}

// 写入客户端的回调
void TcpConnection::on_write(uv_write_t* req, int status) {
	//printf("on_write[%d]\n",status);
	if (status) {
		logger.log(Logger::LogLevel::ERROR,"Write error {}: {}",status,uv_strerror(status));
	}
	free(req);  // 释放写请求
	if (status == UV_ECONNRESET || status == UV_EPIPE) {
		auto connection = static_cast<TcpConnection*>(req->data);
		connection->stop();
	}
}

// 读取客户端数据的回调
void TcpConnection::on_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
	//logger.log(Logger::LogLevel::INFO, "TcpConnection::on_read in");
	auto connection = static_cast<TcpConnection*>(client->data);
	if (nread == 0) {
		std::cerr << "Read error " << uv_strerror(nread) << std::endl;
	}
	else if (nread < 0) {
		logger.log(Logger::LogLevel::ERROR,"Read error({}): {}",nread,uv_strerror(nread));
		connection->stop();
	} else {
#ifdef DEBUG
		printf("[%s]TCP[%d] RECV[%ld]: \r\n", get_timestamp().c_str(), connection->transport_id_,nread);
		print_packet(reinterpret_cast<const uint8_t*>(buf->base),nread);
#endif
		if (connection->transport_ && nread > 0) {
			int ret = connection->transport_->input(buf->base, nread,std::chrono::milliseconds(100));
			if (ret <= 0) {
				logger.log(Logger::LogLevel::WARNING, "write CircularBuffer err {}, {} bytes data discarded",ret,nread);
			}
		} 		
	}
	//logger.log(Logger::LogLevel::INFO, "TcpConnection::on_read out");
}



