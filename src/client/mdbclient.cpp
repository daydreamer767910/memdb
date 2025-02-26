#include <thread>
#include <future>
#include <csignal>
#include "mdbclient.hpp"

MdbClient::ptr MdbClient::my_instance = nullptr;

void MdbClient::set_transport(trans_pair& port_info) {
	transport_ = port_info.second;
	transport_->setCompressFlag(true);
	transport_id_ = port_info.first;
	//Crypt::NoiseKeypair clntNKP;
	//crypt_.loadKeys(user_, clntNKP);
	//transport_->setNoiseKeys(clntNKP.secretKey, clntNKP.publicKey);
}

uint32_t MdbClient::get_transportid() {
	return transport_id_;
}

int MdbClient::Ecdh() {
    auto clientKxPair = generateKxKeypair();
	uint32_t msg_id = 0;
    json jsonData;
    //jsonData["fields"] = json::array({ "nested.details.author", "nested.value" });
    jsonData["action"] = "ECDH";
    jsonData["primitive"] = "HKDF";
    jsonData["pkc"] = toHexString(clientKxPair.first);
    // Convert JSON to string
    std::string jsonConfig = jsonData.dump(1);
    
    // 写操作，支持超时
    int ret = 0;
    ret = this->send(jsonConfig,msg_id, 1000);
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
    jsonConfig = jsonData.dump(1);
    
	ret = this->send(jsonConfig,0, 1000);
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

int MdbClient::reconnect(const std::string& host, const std::string& port) {
	host_ = host.empty() ? host_ : host;
	port_ = port.empty() ? port_ : port;
	if (!connect(host_, port_)) {
		std::cerr << "mdb client connect to server fail" << std::endl;
		return -1;
	}
	
	transport_srv->get_port(transport_id_)->reset(Transport::ChannelType::ALL);
	set_async_read(this->read_buf,sizeof(this->read_buf));
	return 0;
}

int MdbClient::start(const std::string& host, const std::string& port) {
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
	
	
	auto port_info = transport_srv->open_port();
	set_transport(port_info);
	port_info.second->add_callback(my_instance);

	transport_srv->start();

	asio_eventLoopThread = std::thread([this]() {
        //std::cout << "asoi Event loop starting:" << std::this_thread::get_id() << std::endl;
        // Run the loop
        io_context_.run();
        // Clean up
        //std::cout << "asoi Event loop stopped." << std::endl;
    });
	
	//std::cout << "mdb client started." << std::endl;
	return 0;
}

void MdbClient::stop() {
	close();
}

void MdbClient::on_data_received(int result,int) {
	//printf("port to tcp\n");
	//std::cout << "port2tcp thread ID: " << std::this_thread::get_id() << std::endl;
	if (result > 0) {
		int ret = this->write_with_timeout(write_buf,result,50);
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

int MdbClient::send(const std::string& data, uint32_t msg_id, uint32_t timeout) {
	try {
        int ret= transport_->send(reinterpret_cast<const uint8_t*>(data.data()),
						data.size(),
						msg_id,
						std::chrono::milliseconds(timeout));
		//if(ret>0)
		//std::cout << "send to transport:" << ret << std::endl;
		return ret;
    } catch (const std::exception& e) {
        std::cerr << "Send error: " << e.what() << std::endl;
        return -1;
    }
}

int MdbClient::recv(uint8_t* pack_data,uint32_t& msg_id, size_t size,uint32_t timeout) {
	return transport_->read(pack_data,msg_id,size,std::chrono::milliseconds(timeout));
}

void MdbClient::handle_read(const boost::system::error_code& error, std::size_t nread) {
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