#include <thread>
#include <future>
#include <csignal>
#include "transportclient.hpp"

TransportClient::ptr TransportClient::my_instance = nullptr;

int TransportClient::Ecdh() {
    auto clientKxPair = generateKxKeypair();
	uint32_t msg_id = 0;
    json jsonData;
    //jsonData["fields"] = json::array({ "nested.details.author", "nested.value" });
    jsonData["action"] = "ECDH";
    jsonData["primitive"] = "HKDF";
    jsonData["pkc"] = toHexString(clientKxPair.first);
    // Convert JSON to string
    std::string jsonConfig = jsonData.dump();
    
    // 写操作，支持超时
    int ret = 0;
    ret = this->send(reinterpret_cast<const uint8_t*>(jsonConfig.c_str()), jsonConfig.size(),msg_id, 1000);
    if (ret<0) {
        std::cerr << "Write operation failed:" << ret << std::endl;
        return ret;
    }
    
    // 读操作，支持超时
    std::vector<uint8_t> pack_data(1024*5);
    ret = this->recv(pack_data.data(),msg_id, pack_data.size() , 3000);
    if (ret<0 || msg_id!= 0) {
        std::cerr << "ECDH Read operation failed:" << ret << std::endl;
        return ret;
    }
	pack_data.resize(ret);
	try {
		jsonData = json::parse(std::string(pack_data.begin(), pack_data.end()));
	} catch (...) {
		std::cout << "json parse fail:\n" << std::string(pack_data.begin(), pack_data.end()) << std::endl;
		return -3;
	}
	
	#ifdef DEBUG
    std::cout << jsonData.dump(2) << std::endl;
	#endif
    if (jsonData["primitive"] != "HKDF" || jsonData["response"] != "ECDH ACK" || jsonData["status"] != "200") {
        std::cerr << "server err: " << jsonData["response"] << std::endl;
        return -3;
    }
    auto serverPk = hexStringToBytes(jsonData["pks"].get<std::string>());
    auto sessionKeys = generateClientSessionKeys(clientKxPair.first, clientKxPair.second, serverPk);
	if (auto port = transport_.lock()) {
		port->setSessionKeys(sessionKeys.first, sessionKeys.second, true);
		port->setEncryptMode(true);
	} else
		return -5;
    
	//std::cout << "Rx: ";
	//printHex(sessionKeys.first);
	//std::cout << "Tx: ";
	//printHex(sessionKeys.second);
	
	
    //开始PBKDF2请求
	jsonData.clear();
    jsonData["action"] = "ECDH";
    jsonData["primitive"] = "Argon2";
    jsonData["userid"] = this->user_;
	jsonData["password"] = this->passwd_;
    jsonConfig = jsonData.dump();
    
	ret = this->send(reinterpret_cast<const uint8_t*>(jsonConfig.c_str()), jsonConfig.size(),0, 1000);
    if (ret<0) {
        std::cerr << "Write operation failed:" << ret << std::endl;
        return ret;
    }
    // 读操作，支持超时
	pack_data.clear();
	pack_data.resize(1024*5);
    ret = this->recv(pack_data.data(),msg_id, pack_data.size() , 3000);
    if (ret<0 || msg_id!=0) {
        std::cerr << "ECDH Read operation failed:" << ret << std::endl;
        return ret;
    }
	pack_data.resize(ret);
	try {
		jsonData = json::parse(std::string(pack_data.begin(), pack_data.end()));
	} catch (...) {
		std::cout << "json parse fail:\n" << std::string(pack_data.begin(), pack_data.end()) << std::endl;
		return -3;
	}
	#ifdef DEBUG
    std::cout << jsonData.dump(2) << std::endl;
	#endif
    if (jsonData["primitive"] != "Argon2" || jsonData["response"] != "ECDH ACK" || jsonData["status"] != "200") {
        std::cerr << "server err: " << jsonData["response"] << std::endl;
        return -4;
    }
	//auto salt = hexStringToBytes(jsonData["salt"].get<std::string>());
	//auto sessionK_rx = derive_key_with_argon2(sessionKeys.first, this->passwd_, salt);
    //auto sessionK_tx = derive_key_with_argon2(sessionKeys.second, this->passwd_, salt);
	uint64_t keyid = jsonData["keyid"];
	auto sessionK_rx = derive_key_with_password(sessionKeys.first, keyid, this->passwd_);
    auto sessionK_tx = derive_key_with_password(sessionKeys.second, keyid, this->passwd_);
	if (auto port = transport_.lock())
    	port->setSessionKeys(sessionK_rx, sessionK_tx, true);
	else
		return -5;
    return 0;
}

int TransportClient::connect() {
	auto socket = connect(host_, port_);
	if (!socket) {
		std::cerr << "mdb client connect to server fail" << std::endl;
		return -1;
	}
	tranportMng_->start();
	if (io_context_.stopped()) {
        io_context_.restart();  // 重新启动 io_context
    }
	work_guard_ = std::make_shared<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(io_context_.get_executor());
	asio_eventLoopThread = std::thread([this]() {
		io_context_.run();
	});

	id_++;
	tcp_client_ = std::make_shared<TcpConnection>(std::move(*socket), id_);
	tcp_client_->add_observer(shared_from_this());
	tcp_client_->start();
	return this->Ecdh();
}

int TransportClient::start(const std::string& host, const std::string& port) {
	host_ = host.empty() ? host_ : host;
	port_ = port.empty() ? port_ : port;
	
	return connect();
}

void TransportClient::stop() {
	tranportMng_->stop();
	if (tcp_client_)
		tcp_client_->stop();
	tcp_client_ = nullptr;
	io_context_.stop();

	if (asio_eventLoopThread.joinable())
		asio_eventLoopThread.join();
	if (work_guard_) {
		work_guard_->reset();  // 释放 work_guard
	}
}

int TransportClient::send(const uint8_t* data, size_t size, uint32_t msg_id, uint32_t timeout) {
	if (auto port = transport_.lock())
		return port->send(data, size,msg_id,std::chrono::milliseconds(timeout));
	else
		return -2;
}

int TransportClient::recv(uint8_t* pack_data,uint32_t& msg_id, size_t size,uint32_t timeout) {
	if (auto port = transport_.lock())
		return port->read(pack_data,msg_id,size,std::chrono::milliseconds(timeout));
	return -2;
}
