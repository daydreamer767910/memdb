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
#include "dbcore/database.hpp"
#include "net/transport.hpp"



class DbTask: public IDataCallback {
public:
	DbTask(uint32_t port_id, boost::asio::io_context& io_context)
		: port_id_(port_id), io_context_(io_context){
			cached_data_ = std::make_tuple(&data_packet_, max_cache_size, port_id_);
			data_packet_.resize(max_cache_size);  // 预分配缓存大小
	}
	
	~DbTask() {
	#ifdef DEBUG
		std::cout << "DB task " << port_id_ << " destoryed" << std::endl;
	#endif
	}

public:
	DataVariant& get_data() override {
        return cached_data_;
    }
	void on_data_received(int result, int id) override;

	void handle_task(std::shared_ptr<json> json_data, uint32_t msg_id);
private:
	std::vector<uint8_t> data_packet_;//the container to read msg from transport layer
	uint32_t port_id_;
	static constexpr size_t max_cache_size = 32*1024;  
	DataVariant cached_data_;
	boost::asio::io_context& io_context_;
};


#endif