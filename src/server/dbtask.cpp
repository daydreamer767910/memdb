#include "dbtask.hpp"
#include "dbservice.hpp"
#include "registry.hpp"

void DbTask::on_data_received(int result, int id) {
    if (result > 0) {
        try {
            // 限制解析范围，避免解析额外无效数据
            json j = json::parse(std::string(data_packet_.begin(), data_packet_.begin() + result));
    
            auto jsonTask = std::make_shared<json>(j);
            if (auto self = shared_from_this()) {  
                boost::asio::post(io_context_, [self, this, jsonTask, id]() {  
                    this->handle_task(jsonTask, id);
                });
            }
        } catch (...) {
            std::cout << "json parse fail:\n" 
                      << std::string(data_packet_.begin(), data_packet_.begin() + result) 
                      << std::endl;
        }
        // 移除已处理的数据（避免不必要的 clear + resize）
        data_packet_.erase(data_packet_.begin(), data_packet_.begin() + result);
    }    
}

void DbTask::handle_task(std::shared_ptr<json> json_data, uint32_t msg_id) {
    //std::cout << "handle_task in, the memory info:\n";
    //print_memory_usage();
    auto sendResponse = [&](const uint32_t msg_id, const json& response) {
        auto port = TransportSrv::get_instance()->get_port(port_id_);
        if (port) {
            std::string strResp = response.dump();
            int ret = port->send(reinterpret_cast<const uint8_t*>(strResp.data()), 
                            strResp.size(),
                            msg_id, std::chrono::milliseconds(100));
            if (ret < 0) {
                std::cerr << "APP SEND err: " << ret << std::endl;
            }
        }
    };

    
    json jsonResp;
    try {
        auto db = DBService::getInstance()->getDb();
        auto handler = ActionRegistry::getInstance().getHandler((*json_data)["action"]);
        handler->port_id_ = port_id_;
        handler->handle(*json_data, db, jsonResp);
    } catch (const std::exception& e) {
        jsonResp["error"] = e.what();
    } catch (...) {
        jsonResp["error"] = "Unknown exception occurred!";
    }
    sendResponse(msg_id, jsonResp);
    
    //std::cout << "handle_task out, the memory info:\n";
    //print_memory_usage();
}
