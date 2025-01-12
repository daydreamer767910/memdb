#include <thread>
#include <future>
#include <csignal>
#include "mdbclient.hpp"

MdbClient::ptr MdbClient::my_instance = nullptr;

void MdbClient::set_transport(trans_pair& port_info) {
	transport_ = port_info.second;
	transport_id_ = port_info.first;
}

uint32_t MdbClient::get_transportid() {
	return transport_id_;
}

int MdbClient::reconnect(const std::string& host, const std::string& port) {
	host_ = host.empty() ? host_ : host;
	port_ = port.empty() ? port_ : port;
	if (!connect(host_, port_)) {
		std::cerr << "mdb client connect to server fail" << std::endl;
		return -1;
	}
	set_async_read(this->read_buf,sizeof(this->read_buf));
	return 0;
}

int MdbClient::start(const std::string& host, const std::string& port) {
	// 连接到服务器
	host_ = host;
	port_ = port;
	if (!connect(host_, port_)) {
		std::cerr << "mdb client connect to server fail" << std::endl;
		return -1;
	}
	set_async_read(this->read_buf,sizeof(this->read_buf));
	
	uv_loop_init(&loop);
	auto port_info = transport_srv->open_port(&loop);
	set_transport(port_info);
	port_info.second->add_callback(my_instance);

	std::promise<void> promise;
    std::future<void> future = promise.get_future();
	// Run the event loop in a new thread
    uv_eventLoopThread = std::thread([this](std::promise<void> promise) {
        uv_signal_t sig;
        // Set up signal listener
        uv_signal_init(&loop, &sig);
        uv_signal_start(&sig, [](uv_signal_t* handle, int signum) {
            std::cout << "Signal received: " << signum << std::endl;
            uv_stop(handle->loop); // Stop the loop
            uv_signal_stop(handle); // Stop the signal listener
            uv_close((uv_handle_t*)handle, nullptr); // Free resources
        }, SIGUSR1); // Capture SIGINT signal

        promise.set_value(); // Notify the main thread
        std::cout << "Event loop starting" << std::endl;

        // Run the loop
        uv_run(&loop, UV_RUN_DEFAULT);

        // Clean up
        std::cout << "Event loop stopped." << std::endl;
        uv_loop_close(&loop);
    }, std::move(promise));
	future.wait(); // 等待子线程通知

	asio_eventLoopThread = std::thread([this]() {
        std::cout << "asoi Event loop starting" << std::endl;
        // Run the loop
        io_context_.run();
        // Clean up
        std::cout << "asoi Event loop stopped." << std::endl;
    });
	
	std::cout << "mdb client started." << std::endl;
	return 0;
}

void MdbClient::stop() {
	close();
	io_context_.stop();
	transport_srv->close_port(this->transport_id_);
	uv_stop(&loop);
	pthread_t threadId = uv_eventLoopThread.native_handle();
	pthread_kill(threadId, SIGUSR1);
	if (uv_eventLoopThread.joinable())
		uv_eventLoopThread.join();
	if (asio_eventLoopThread.joinable())
		asio_eventLoopThread.join();
}

void MdbClient::on_data_received(int result) {
	//printf("port to tcp\n");
	if (result > 0) {
		int ret = this->write_with_timeout(write_buf,result,1);
		if ( ret == -2 ) {
			//close();
			reconnect();
		}
	}
}

int MdbClient::send(const std::string& data, uint32_t msg_id, uint32_t timeout) {
	try {
        return transport_->send(data,4,std::chrono::milliseconds(timeout));
    } catch (const std::exception& e) {
        std::cerr << "Send error: " << e.what() << std::endl;
        return false;
    }
}

int MdbClient::recv(std::vector<json>& json_datas, uint32_t timeout) {
	return transport_->read(json_datas,std::chrono::milliseconds(timeout));
}

void MdbClient::handle_read(const boost::system::error_code& error, std::size_t nread) {
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
				//close();
				if (reconnect()<0)
					return;
		}
	}
	set_async_read(this->read_buf,sizeof(this->read_buf));
}