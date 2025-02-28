#include "transportsrv.hpp"

int TransportSrv::start(std::string ip, uint32_t port) {
	tranportMng_->start();
	TcpServer::start(ip, port);
	return 0;
}

void TransportSrv::stop() {
	tranportMng_->stop();
	TcpServer::stop();
}

void TransportSrv::on_new_connection(const std::shared_ptr<IConnection<Transport>>& connection, const uint32_t id) {
	auto port = tranportMng_->open_port(id);
    port->add_callback(connection);
    connection->set_transport(port);
	this->notify_new_transport(port, id);
}

void TransportSrv::on_close_connection(const uint32_t id) {
	tranportMng_->close_port(id);
	this->notify_close_transport(id);
}