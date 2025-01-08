#include "tcpconnection.hpp"
#include "log/logger.hpp"
#include "util/util.hpp"

void TcpConnection::start(uv_tcp_t* client, int transport_id) {
	client_ = client;
	client_->data = this;
	transport_id_ = transport_id;
	keep_alive_cnt = 0;
	timer.start();
	TransportSrv::get_instance().reset(transport_id_);
	uv_read_start((uv_stream_t*)client_, on_alloc_buffer, on_read);

	status_ = 1;

	logger.log(Logger::LogLevel::INFO,"connection started");
}

void TcpConnection::stop() {
	status_ = 0;
	keep_alive_cnt = 0;
	timer.stop();
	
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
	printf("[%s]TCP[%d] SEND: \r\n", get_timestamp().c_str(), transport_id_);
	print_packet(reinterpret_cast<const uint8_t*>(data),length);
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
	auto* connection = static_cast<TcpConnection*>(client->data);
	if (nread < 0) {
		logger.log(Logger::LogLevel::ERROR,"Read error({}): {}",nread,uv_strerror(nread));
		if (nread == UV_EOF) connection->stop();
	} else {
		printf("[%s]TCP[%d] RECV: \r\n", get_timestamp().c_str(), connection->transport_id_);
		print_packet(reinterpret_cast<const uint8_t*>(buf->base),nread);
		int ret = TransportSrv::get_instance().input(buf->base, nread,connection->transport_id_,1000);
		if (ret == 0) {
			logger.log(Logger::LogLevel::ERROR, "transport is unavailable for TCP recv");
			//connection->write("internal err",12);
		}
		else if (ret < 0) {
			logger.log(Logger::LogLevel::WARNING, "CircularBuffer full, data discarded");
			//connection->write("buffer full, data discarded",27);
		} else {
			//connection->write("ack",3);
		}
		connection->keep_alive_cnt = 0;
	}
	//logger.log(Logger::LogLevel::INFO, "TcpConnection::on_read out");
}

void TcpConnection::on_poll(uv_poll_t* handle) {
	Transport* port = static_cast<Transport*>(handle->data);
	int ret = port->output(write_buf,sizeof(write_buf),std::chrono::milliseconds(50));
	if (ret ==0 ) {
		logger.log(Logger::LogLevel::ERROR, "Port[{}] is unavailable",transport_id_);
		//stop();
	} else if (ret>0) {
		//send to client socket
		write(write_buf,ret);
		keep_alive_cnt = 0;
	} else {
		//timeout or buffer empty
	}
}

void TcpConnection::on_timer() {
	if(keep_alive_cnt>2) {
		logger.log(Logger::LogLevel::INFO, "No keepalive resp {} times, kick off the client",keep_alive_cnt-1);
		this->stop();
	} else if ( keep_alive_cnt>0) {
		logger.log(Logger::LogLevel::INFO,"Processing keep alive {}",keep_alive_cnt);
		write("keep alive checking", 20);
	}
	keep_alive_cnt++;

}
