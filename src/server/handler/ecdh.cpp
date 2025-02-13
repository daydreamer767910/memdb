#include "../registry.hpp"
#include "keymng/crypt.hpp"
#include "net/transportsrv.hpp"

class EcdhHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
		std::string userId = task["userid"];
		std::string primitive = task["primitive"];
		uint32_t portId = task["portId"];//filled by Task
		// 准备响应
        response["response"] = "ECDH ACK";
        response["status"] = "200";
		
		if (primitive == "PKE") {//public key exchange
			auto userPsk = hexStringToBytes(task["psk"].get<std::string>());
			std::vector<uint8_t> salt(crypto_pwhash_SALTBYTES);
			randombytes_buf(salt.data(), salt.size());  // 生成随机 salt
			auto serverKxPair = generateKxKeypair();

			auto container = db->getContainer("users");
			if (container == nullptr) {
				response["response"] = "Container users not found";
				response["status"] = "404";
				return;
			}
			if (container->getType() == "table") {
				auto tb = std::dynamic_pointer_cast<Table>(container);
				//tbd
			} else if (container->getType() == "collection") {
				auto collection = std::dynamic_pointer_cast<Collection>(container);
				auto document = collection->getDocument(std::hash<std::string>{}(userId));
				if (!document) {
					std::cerr << "user " << userId << " is missing!!\n";
					response["response"] = "user not found";
					response["status"] = "405";
					return;
				}
				Field* field = document->getFieldByPath("password");
				if (!field) {
					std::cerr << "fail to get password \n";
					response["response"] = "user info err";
					response["status"] = "406";
					return;
				}
				std::string passwd = std::get<std::string>(field->getValue());
				auto sessionKs = generateServerSessionKeys(serverKxPair.first, serverKxPair.second, userPsk);
				
				auto sessionK_rx = derive_key_with_argon2(sessionKs.first, passwd, salt);
				auto sessionK_tx = derive_key_with_argon2(sessionKs.second, passwd, salt);

				document->setField("sessionK_rx", Field(sessionK_rx));
				document->setField("sessionK_tx", Field(sessionK_tx));
				
				response["primitive"] = "PKE";
				response["psk"] = toHexString(serverKxPair.first);
				response["salt"] = toHexString(salt);
			}
		} else if (primitive == "KDF") {//Key Derivation Function
			auto container = db->getContainer("users");
			if (container == nullptr) {
				response["response"] = "Container users not found";
				response["status"] = "404";
				return;
			}
			if (container->getType() == "table") {
				auto tb = std::dynamic_pointer_cast<Table>(container);
				//tbd
			} else if (container->getType() == "collection") {
				auto collection = std::dynamic_pointer_cast<Collection>(container);
				auto document = collection->getDocument(std::hash<std::string>{}(userId));
				if (!document) {
					std::cerr << "user " << userId << " is missing!!\n";
					response["response"] = "user not found";
					response["status"] = "405";
					return;
				}
				Field* field = document->getFieldByPath("sessionK_rx");
				if (!field) {
					std::cerr << "fail to get sessionK_rx \n";
					response["response"] = "sessionK_rx info err";
					response["status"] = "406";
					return;
				}
				auto sessionK_rx = std::get<std::vector<uint8_t>>(field->getValue());
				field = document->getFieldByPath("sessionK_tx");
				if (!field) {
					std::cerr << "fail to get sessionK_tx \n";
					response["response"] = "sessionK_tx info err";
					response["status"] = "406";
					return;
				}
				auto sessionK_tx = std::get<std::vector<uint8_t>>(field->getValue());

				auto port = TransportSrv::get_instance()->get_port(portId);
				if (port) {
					port->setSessionKeys(sessionK_rx, sessionK_tx);
				}
				response["primitive"] = "KDF";
			}
		}
    }
};

REGISTER_ACTION("ECDH", EcdhHandler)