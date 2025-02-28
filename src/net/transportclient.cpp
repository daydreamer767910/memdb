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
        std::cerr << "Read operation failed:" << ret << std::endl;
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
    transport_->setSessionKeys(sessionKeys.first, sessionKeys.second, true);
	transport_->setEncryptMode(true);
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
        std::cerr << "Read operation failed:" << ret << std::endl;
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
    transport_->setSessionKeys(sessionK_rx, sessionK_tx, true);

    return 0;
}

int TransportClient::reconnect(const std::string& host, const std::string& port) {
	host_ = host.empty() ? host_ : host;
	port_ = port.empty() ? port_ : port;
	if (!connect(host_, port_)) {
		std::cerr << "mdb client connect to server fail" << std::endl;
		return -1;
	}
	transport_->setEncryptMode(false);
	transport_->reset(Transport::ChannelType::ALL);
	set_async_read(this->read_buf,sizeof(this->read_buf));
	return this->Ecdh();
}

int TransportClient::start(const std::string& host, const std::string& port) {
	static bool started = false;
	if(started) {
		stop();
		return reconnect(host,port);
	}
	started = true;
	// 连接到服务器
	host_ = host;
	port_ = port;
	if (!connect(host_, port_)) {
		std::cerr << "mdb client connect to server fail" << std::endl;
		return -1;
	}
	set_async_read(this->read_buf,sizeof(this->read_buf));
	//std::cout << "Main thread ID: " << std::this_thread::get_id() << std::endl;
		
	tranportMng_->on_new_item(my_instance, id_);
	
	tranportMng_->start();

	asio_eventLoopThread = std::thread([this]() {
        //std::cout << "asoi Event loop starting:" << std::this_thread::get_id() << std::endl;
        // Run the loop
        io_context_.run();
        // Clean up
        //std::cout << "asoi Event loop stopped." << std::endl;
    });
	
	//std::cout << "mdb client started." << std::endl;
	return this->Ecdh();
}

void TransportClient::stop() {
	close();
}

void TransportClient::on_data_received(int len,int) {
	//printf("port to tcp\n");
	//std::cout << "port2tcp thread ID: " << std::this_thread::get_id() << std::endl;
	if (len > 0) {
		int ret = this->write_with_timeout(write_buf,len,50);
		if ( ret == -2 ) {
			close();
			reconnect();
		}
		#ifdef DEBUG
		if(ret>0)
		print_packet(reinterpret_cast<const uint8_t*>(write_buf),ret);
		#endif
	}
}

int TransportClient::send(const uint8_t* data, size_t size, uint32_t msg_id, uint32_t timeout) {
	return transport_->send(data, size,msg_id,std::chrono::milliseconds(timeout));
}

int TransportClient::recv(uint8_t* pack_data,uint32_t& msg_id, size_t size,uint32_t timeout) {
	return transport_->read(pack_data,msg_id,size,std::chrono::milliseconds(timeout));
}

void TransportClient::handle_read(const boost::system::error_code& error, std::size_t nread) {
	//std::cout << "tcp read thread ID: " << std::this_thread::get_id() << std::endl;
	if (!error) {
		//std::cout << "handle_read: " << nread << std::endl;
		int ret = transport_->input(read_buf, nread,std::chrono::milliseconds(100));
		if (ret <= 0) {
			std::cerr << "write CircularBuffer err, data discarded!" << std::endl;
		}
	} else {
		std::cerr << "Error on receive: " << error.message() << std::endl;
		if (error == boost::asio::error::eof || error == boost::asio::error::connection_reset
			|| error == boost::asio::error::not_connected || error ==boost::asio::error::bad_descriptor){
				close();
				if (reconnect()<0)
					return;
		}
	}
	// 仅在连接成功后设置异步读取
    if (is_connected()) {
        set_async_read(this->read_buf, sizeof(this->read_buf));
    } else {
        std::cerr << "Socket is closed, cannot set async read." << std::endl;
    }
}