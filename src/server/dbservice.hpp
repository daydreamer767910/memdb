#ifndef DBSERVICE_HPP
#define DBSERVICE_HPP

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
#include "net/transport.hpp"

class DBService : public ThreadBase<int>{
public:
	explicit DBService() :channel_(4096) {
		
	}
	
private:
    Transport channel_;
	virtual void process() override{
        while (true) {
			json jsonData;
            if(channel_.appReceive(jsonData,std::chrono::milliseconds(500))) {
				std::cout << "appReceive:" << jsonData.dump(4) << std::endl;
				MemDatabase::ptr& db = MemDatabase::getInstance();
				static uint32_t id = 0;
				auto users = db->getTable("users");
				Field userName = std::string("Alice");
				const auto& queryRows = users->queryByIndex("name", userName);
				//std::cout << "query Alice:" << users->rowsToJson(queryRows).dump(4) << std::endl;
				channel_.appSend(users->rowsToJson(queryRows),id++,std::chrono::milliseconds(1500));
			}
        }
    }
};

#endif
