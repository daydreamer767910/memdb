#include "dbtask.hpp"
#include "dbservice.hpp"
#include "registry.hpp"

void DbTask::on_data_received(int result) {
    //static std::atomic<uint32_t> msg_id(0);
    if (result > 0) {
        auto json_datasCopy = std::make_shared<std::vector<json>>(json_datas_);
        auto self = shared_from_this();  // 确保对象生命周期
        boost::asio::post(io_context_, [self, this, json_datasCopy]() {
            this->handle_task(json_datasCopy);
        });
        json_datas_.clear();
    }
}

void DbTask::handle_task(std::shared_ptr<std::vector<json>> json_datas) {
    //std::cout << "handle_task in, the memory info:\n";
    //print_memory_usage();
    auto sendResponse = [&](const uint32_t msg_id, const json& response) {
        auto port = TransportSrv::get_instance()->get_port(port_id_);
        if (port) {
            int ret = port->send(response.dump(), msg_id, std::chrono::milliseconds(100));
            if (ret < 0) {
                std::cerr << "APP SEND err: " << ret << std::endl;
            }
        }
    };

    for (const auto& jsonTask : *json_datas) {
        json jsonResp;
        uint32_t msg_id = 0;
        if (jsonTask.contains("_msg_id")) {
            msg_id = jsonTask["_msg_id"];
        }
        try {
			auto db = DBService::getInstance()->getDb();
			auto handler = ActionRegistry::getInstance().getHandler(jsonTask["action"]);
            handler->port_id_ = port_id_;
			handler->handle(jsonTask, db, jsonResp);
        } catch (const std::exception& e) {
            jsonResp["error"] = e.what();
        } catch (...) {
            jsonResp["error"] = "Unknown exception occurred!";
        }
        sendResponse(msg_id, jsonResp);
    }
    //std::cout << "handle_task out, the memory info:\n";
    //print_memory_usage();
}
