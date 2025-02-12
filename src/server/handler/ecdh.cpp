#include "../registry.hpp"
#include "keymng/crypt.hpp"
#include "net/transportsrv.hpp"

class EcdhHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
		std::string primitive = task["primitive"];
		uint32_t portId = task["portId"];//filled by Task
		// 准备响应
        response["response"] = "ECDH ACK";
        response["status"] = "200";
		
		if (primitive == "PKE") {//public key exchange
			auto pk_C = hexStringToBytes(task["pkc"].get<std::string>());
			auto serverKxPair = generateKxKeypair();
			auto shareKeys = generateServerSessionKeys(serverKxPair.first, serverKxPair.second, pk_C);
			auto port = TransportSrv::get_instance()->get_port(portId);
			if (port) {
				port->setSessionKeys(shareKeys.first, shareKeys.second);
			}	
			response["primitive"] = "PKE";
			response["pks"] = toHexString(serverKxPair.first);
		} else if (primitive == "KDF") {//Key Derivation Function
			std::string userId = task["userid"];
			auto container = db->getContainer("users");
			if (container == nullptr) {
				response["response"] = "Container users not found";
				response["status"] = "404";
				return;
			}
			if (container->getType() == "collection") {
				auto collection = std::dynamic_pointer_cast<Collection>(container);
				auto document = collection->getDocument(std::hash<std::string>{}(userId));
				if (!document) {
					std::cerr << "user " << userId << " is missing!!\n";
					response["response"] = "user not found";
					response["status"] = "405";
					return;
				}
				std::vector<uint8_t> salt(crypto_pwhash_SALTBYTES);
				randombytes_buf(salt.data(), salt.size());  // 生成随机 salt
				std::vector<uint8_t> shareKey_rx, shareKey_tx;
				auto port = TransportSrv::get_instance()->get_port(portId);
				if (port) {
					port->getSessionKeys(shareKey_rx, shareKey_tx);
					auto sessionK_rx = derive_key_with_argon2(shareKey_rx, userId, salt);
					auto sessionK_tx = derive_key_with_argon2(shareKey_tx, userId, salt);
					port->setSessionKeys(sessionK_rx, sessionK_tx);
				}
				response["salt"] = toHexString(salt);
				response["primitive"] = "KDF";
			}
		}
    }
};

REGISTER_ACTION("ECDH", EcdhHandler)