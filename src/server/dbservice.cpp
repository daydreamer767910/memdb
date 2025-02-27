#include "dbservice.hpp"
#include "log/logger.hpp"
#include "net/transportmng.hpp"
#include "util/util.hpp"

DBService::DBService() :thread_pool_(std::thread::hardware_concurrency()/2),
	io_(),
	db(Database::getInstance()),
	timer(io_, keep_alv_timer, true, [this](int tick, int time, std::thread::id id) {
        this->on_timer(tick,time,id);
	}),
	work_guard_(boost::asio::make_work_guard(io_)) {
	std::cout << "DBService start" << std::endl;
	load_db();
	std::string user, pwd;
	load_users(user, pwd);
	// 启动定时器
	timer.start();

	// 启动事件循环
	//std::cout << "DBService thread pool started with " << std::thread::hardware_concurrency()/2 << " threads." << std::endl;
	// 在线程池中运行 io_context
	for (std::size_t i = 0; i < std::thread::hardware_concurrency(); ++i) {
		boost::asio::post(thread_pool_, [this]() {
			io_.run();
		});
	}
	
	
}

DBService::~DBService() {
	// 停止事件循环
    io_.stop();
	
    // 停止线程池
    thread_pool_.stop();
	//std::cout << "DBService thread pool stoped\n";
	work_guard_.reset();
    // 等待线程池中的所有线程完成任务
    thread_pool_.join();
	#ifdef DEBUG
	std::cout << "DBService thread pool joined\n";
	#endif
    // 清理任务
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.clear();  // 清空 map
    }
	#ifdef DEBUG
    std::cout << "DBtasks stopped" << std::endl;
	#endif
    // 保存数据库
    save_db();
    //std::cout << "DBService saved" << std::endl;

    #ifdef DEBUG
    std::cout << "DB service destroyed!" << std::endl;
    #endif
}

void DBService::on_msg(const std::shared_ptr<DBVariantMsg> msg) {
	std::visit([this](auto&& message) {
		std::lock_guard<std::mutex> lock(mutex_);
        auto [jsonDatas, port_id, msg_id] = message;
		auto it = tasks_.find(port_id);
		if (it != tasks_.end()) {
			// 显式捕获 msg_id 和 jsonDatas
            boost::asio::post(io_, [task = it->second, jsonDatas,msg_id]() {
                task->handle_task(jsonDatas,msg_id);
            });
		}
    }, *msg);
}

void DBService::keep_alive() {
	auto port_ids = TransportMng::get_instance()->get_all_ports();
	std::cout << " Timer thread id: " << std::this_thread::get_id() <<std::endl;
	json jsonData;
	for (int port_id : port_ids) {
		jsonData["type"] = "keep alive(s)";
		jsonData["timeout"] = keep_alv_timer/1000;
		auto port = TransportMng::get_instance()->get_port(port_id);
		if (port) {
			std::string strData = jsonData.dump();
			int ret = port->send( reinterpret_cast<const uint8_t*>(strData.data()), 
								strData.size(),
								0xffffffff ,std::chrono::milliseconds(100));
			if (ret<0) {
				std::cout << "APP SEND err:" << ret <<  std::endl;
			}
		}
	}
}

void DBService::load_users(std::string& admin, std::string& pwd) {
	admin = "admin";
	pwd = "admin";
	DataContainer::ptr container = db->getContainer("users");
	if (!container) {
		std::cerr << "user container is missing!!\n";
		return;
	} else {
		auto collection = std::dynamic_pointer_cast<Collection>(container);
		auto document = collection->getDocument(std::hash<std::string>{}("admin"));
		if (!document) {
			std::cerr << "user admin is missing!!\n";
			return;
		}
		Field* field = document->getFieldByPath("password");
		if (!field) {
			std::cerr << "fail to get password \n";
			return;
		}
		pwd = std::get<std::string>(field->getValue());
	}
}

void DBService::save_db() {
	std::string baseDir = std::string(std::getenv("HOME"));
	std::filesystem::path fullPath = std::filesystem::path(baseDir) / std::string("data");
//std::cout << "DBService::on_timer :save db to" << fullPath.string() << std::endl;
	db->save(fullPath.string());
}

void DBService::load_db() {
	std::string baseDir = std::string(std::getenv("HOME"));
	std::filesystem::path fullPath = std::filesystem::path(baseDir) / std::string("data");
	db->upload(fullPath.string());
}

void DBService::on_timer(int , int , std::thread::id ) {
	//save_db();
	//keep_alive();
}

