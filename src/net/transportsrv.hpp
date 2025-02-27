#ifndef TransportSrv_HPP
#define TransportSrv_HPP

#include "transport.hpp"
#include "transportmng.hpp"
#include "tcpserver.hpp"
class TransportSrv {
public:
	TransportSrv() {
		tranportMng_ = TransportMng::get_instance();
		tcpSrv_.add_observer(tranportMng_);
	}
	void add_observer(const std::shared_ptr<ITransportObserver>& observer);
	int start(std::string ip, uint32_t port);
	void stop();
private:
	TransportMng::ptr tranportMng_;
	TcpServer tcpSrv_;
};

#endif