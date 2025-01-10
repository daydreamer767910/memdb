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
#include <variant>
#include "dbcore/memdatabase.hpp"
#include "net/transport.hpp"

class DbTask: public IDataCallback{
public:
	DbTask(uint32_t port_id):port_id_(port_id) {
	}
	~DbTask() {
		std::cout << "DB task exit" << std::endl;
	}

private:
	void handle_task();
public:
	DataVariant& get_data() override {
		//std::cout << "clear json datas\n";
		json_datas.clear();
		cached_data_ = std::make_tuple(&json_datas, port_id_);
        return cached_data_;
    }
	void on_data_received(int result) override {
		if (result >0 )
        	handle_task();
    }
private:
	std::vector<json> json_datas;//the container to read msg from transport layer
	uint32_t port_id_;
	DataVariant cached_data_;

};


#endif