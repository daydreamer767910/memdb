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
#include "dbcore/memdatabase.hpp"
#include "net/transport.hpp"



class DbTask: public IDataCallback {
public:
	DbTask(uint32_t port_id, boost::asio::io_context& io_context)
		: port_id_(port_id), io_context_(io_context){
	}
	~DbTask() {
	#ifdef DEBUG
		std::cout << "DB task " << port_id_ << " destoryed" << std::endl;
	#endif
	}

public:
	DataVariant& get_data() override {
		json_datas.clear();
		cached_data_ = std::make_tuple(&json_datas, max_cache_size, port_id_);
        return cached_data_;
    }
	void on_data_received(int result) override;

	void handle_task(uint32_t msg_id, std::shared_ptr<std::vector<json>> json_datasCopy);
private:
	std::vector<json> json_datas;//the container to read msg from transport layer
	uint32_t port_id_;
	static constexpr size_t max_cache_size = 8;  // 缓存的最大消息数量
	DataVariant cached_data_;
	boost::asio::io_context& io_context_;
};


#endif