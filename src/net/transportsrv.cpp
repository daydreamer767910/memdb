#include "transportsrv.hpp"

int TransportSrv::start(std::string ip, uint32_t port) {
	tranportMng_->start();
	tcpSrv_.start(ip, port);
	return 0;
}

void TransportSrv::stop() {
	tranportMng_->stop();
	tcpSrv_.stop();
}

void TransportSrv::add_observer(const std::shared_ptr<ITransportObserver>& observer) {
	tranportMng_->add_observer(observer);
}