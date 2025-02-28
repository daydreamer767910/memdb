#ifndef TransportSrv_HPP
#define TransportSrv_HPP

#include "transportmng.hpp"
#include "tcpserver.hpp"
class TransportSrv: public TcpServer {
public:
	TransportSrv() {
		tranportMng_ = TransportMng::get_instance();
	}
	void add_observer(const std::shared_ptr<IObserver<Transport>>& observer) {
        observers_.push_back(observer);
    }
	int start(std::string ip, uint32_t port);
	void stop();
private:
	void notify_new_transport(const std::shared_ptr<Transport>& transport, const uint32_t id) {
		for (const auto& observer : observers_) {
			observer->on_new_item(transport, id);
		}
	}
	void notify_close_transport(const uint32_t id) {
		for (const auto& observer : observers_) {
			observer->on_close_item(id);
		}
	}
	void on_new_connection(const std::shared_ptr<TcpConnection>& connection, const uint32_t id) override;
	void on_close_connection(const uint32_t id) override;
	TransportMng::ptr tranportMng_;
	std::vector<std::shared_ptr<IObserver<Transport>>> observers_;
};

#endif