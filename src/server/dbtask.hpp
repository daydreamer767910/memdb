#ifndef DBTASK_HPP
#define DBTASK_HPP

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <queue>
#include <mutex>
#include <variant>
#include <atomic>
#include <cstdint>
#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include "net/transport.hpp"
class DbTask: public IConnection {
public:
	DbTask(uint32_t id, boost::asio::io_context& io_context)
		: id_(id), io_context_(io_context){
			
	}
	
	~DbTask() {
	#ifdef DEBUG
		std::cout << "DB task " << id_ << " destoryed" << std::endl;
	#endif
	}
	uint32_t get_id() override{
        return id_;
    }
	void set_transport(const std::shared_ptr<Transport>& transport) override {
        transport_ = transport;
		id_ = transport->get_id();
		cached_data_ = std::make_tuple(&data_packet_, transport->getMessageSize(), id_);
		data_packet_.resize(transport->getMessageSize());  // 预分配缓存大小
		transport->setCompressFlag(true);
    }
	DataVariant& get_data() override {
        return cached_data_;
    }
	void on_data_received(int len, int msg_id) override;

	void handle_task(std::shared_ptr<json> json_data, uint32_t msg_id);
private:
	std::vector<uint8_t> data_packet_;//the container to read msg from transport layer
	uint32_t id_;
	std::weak_ptr<Transport> transport_;
	DataVariant cached_data_;
	boost::asio::io_context& io_context_;
};


#endif