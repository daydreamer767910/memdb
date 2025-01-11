#include "mdbclient.hpp"


void MdbClient::start(ptr client_ptr) {
	client_ptr->transport_srv_ = TransportSrv::get_instance();
	// 连接到服务器
	while (!client_ptr->connect(client_ptr->host_, client_ptr->port_)) {
		std::cerr << "mdb client connect to server fail" << std::endl;
		sleep(3);
	}
	auto port_info = client_ptr->transport_srv_->open_port();

	port_info.second->add_callback(client_ptr);
	
	client_ptr->transport_id_ = port_info.first;
	client_ptr->transport_ = port_info.second;
	// Run the event loop
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}

void MdbClient::stop() {
	transport_srv_->close_port(transport_id_);
	this->close();
	uv_stop(uv_default_loop());
}

void MdbClient::on_data_received(int result) {
	if (result > 0) {
		if (this->write_with_timeout(write_buf,result,1) < 0) {
			std::cerr << "write_with_timeout err" << std::endl;
		}
	}
}

int MdbClient::send(const json& json_data, uint32_t msg_id, uint32_t timeout) {
	try {
        return transport_->send(json_data,4,std::chrono::milliseconds(timeout));
    } catch (const std::exception& e) {
        std::cerr << "Send error: " << e.what() << std::endl;
        return false;
    }
}

int MdbClient::recv(std::vector<json>& json_datas, uint32_t timeout) {
	int nread = read_with_timeout(read_buf,sizeof(read_buf),1);
	if ( nread > 0) {
		int ret = transport_->input(read_buf, nread,std::chrono::milliseconds(timeout));
		if (ret <= 0) {
			std::cerr << "write CircularBuffer err" << std::endl;
			return ret;
		}
		return transport_->read(json_datas,std::chrono::milliseconds(timeout));
	}
	return nread;
}