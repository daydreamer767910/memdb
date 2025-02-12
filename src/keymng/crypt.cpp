#include "crypt.hpp"

// 生成Noise协议密钥对
std::pair<std::vector<unsigned char>, std::vector<unsigned char>> generateNoiseKeypair() {
    std::vector<unsigned char> publicKey(crypto_box_PUBLICKEYBYTES);
    std::vector<unsigned char> secretKey(crypto_box_SECRETKEYBYTES);

    crypto_box_keypair(publicKey.data(), secretKey.data());  // 生成密钥对
    return {publicKey, secretKey};
}

// 加密函数，使用指定的密码加密数据
void encryptData(const std::vector<unsigned char>& key, 
	const std::vector<unsigned char>& data, 
	std::vector<unsigned char>& ciphertext) {

    std::vector<unsigned char> nonce(crypto_secretbox_NONCEBYTES);
    randombytes_buf(nonce.data(), crypto_secretbox_NONCEBYTES);  // 随机生成nonce
    
    ciphertext.resize(crypto_secretbox_MACBYTES + crypto_secretbox_NONCEBYTES + data.size());
    std::memcpy(ciphertext.data(), nonce.data(), nonce.size()); // 将nonce附加到密文的开头

    if (crypto_secretbox_easy(ciphertext.data() + crypto_secretbox_NONCEBYTES, 
        data.data(), 
        data.size(), 
        nonce.data(), 
        key.data()) != 0) {
        std::cerr << "encryptData fail" << std::endl;
    }
}

// 解密函数，使用指定的密码解密数据
bool decryptData(const std::vector<unsigned char>& key, 
	const std::vector<unsigned char>& ciphertext, 
	std::vector<unsigned char>& decryptedData) {

    std::vector<unsigned char> nonce(crypto_secretbox_NONCEBYTES);
    std::memcpy(nonce.data(), ciphertext.data(), crypto_secretbox_NONCEBYTES); // 从密文中提取nonce
    auto ciphertextLen = ciphertext.size() - crypto_secretbox_NONCEBYTES;
    auto decryptedDataLen = ciphertext.size() - crypto_secretbox_MACBYTES - crypto_secretbox_NONCEBYTES;
    decryptedData.resize(decryptedDataLen);
    int result = crypto_secretbox_open_easy(decryptedData.data(), 
        ciphertext.data() + crypto_secretbox_NONCEBYTES, 
        ciphertextLen,
        nonce.data(), 
        key.data());

    if (result != 0) {
        std::cerr << "decryptData fail: " << result << std::endl;
        return false;
    }
    return true;
}

void encryptDataWithNoiseKey(
    const std::vector<unsigned char>& senderSecretKey,
    const std::vector<unsigned char>& receiverPublicKey,
    const std::vector<unsigned char>& plaintext,
    std::vector<unsigned char>& ciphertext) {

    std::vector<unsigned char> nonce(crypto_box_NONCEBYTES);
    randombytes_buf(nonce.data(), nonce.size()); // 生成随机 nonce

    ciphertext.resize(crypto_box_MACBYTES + nonce.size() + plaintext.size());

    // 复制 nonce 到密文开头
    std::memcpy(ciphertext.data(), nonce.data(), nonce.size());

    if (crypto_box_easy(
            ciphertext.data() + nonce.size(),
            plaintext.data(), plaintext.size(),
            nonce.data(),
            receiverPublicKey.data(),
            senderSecretKey.data()) != 0) {
        std::cerr << "encryptDataWithNoiseKey fail" << std::endl;
    }
}

bool decryptDataWithNoiseKey(
    const std::vector<unsigned char>& senderPublicKey,
    const std::vector<unsigned char>& receiverSecretKey,
    const std::vector<unsigned char>& ciphertext,
    std::vector<unsigned char>& decryptedData) {

    std::vector<unsigned char> nonce(crypto_box_NONCEBYTES);
    std::memcpy(nonce.data(), ciphertext.data(), nonce.size());

    size_t ciphertextLen = ciphertext.size() - crypto_box_NONCEBYTES;
    decryptedData.resize(ciphertextLen - crypto_box_MACBYTES);

    int result = crypto_box_open_easy(
        decryptedData.data(),
        ciphertext.data() + nonce.size(), ciphertextLen,
        nonce.data(),
        senderPublicKey.data(),
        receiverSecretKey.data());

    if (result != 0) {
        std::cerr << "decryptDataWithNoiseKey fail" << std::endl;
        return false;
    }
    return true;
}

namespace Transport_Crypt {

bool Crypt::init(const Database::ptr& db, const std::string& password) {
	password_ = password;
	salt_.resize(32);
	mainKey_.resize(32);
	randombytes_buf(salt_.data(), salt_.size());  // 生成随机 salt
	randombytes_buf(mainKey_.data(), mainKey_.size());  // 使用随机数生成主密钥
	if (loadMainKey(db)) {
		if (!loadServerKey(db) || !loadClientKey(db)) {
			return false;
		} else {
			return true;
		}
	}
	return false;
}

std::vector<unsigned char> Crypt::deriveKeyFromPassword() {
	std::vector<unsigned char> key(32);
    auto result = crypto_pwhash_scryptsalsa208sha256(
        key.data(), key.size(),
        password_.c_str(), password_.size(),
        salt_.data(), 100000, crypto_pwhash_scryptsalsa208sha256_STRBYTES);

    if (result != 0) {
        std::cerr << "deriveKeyFromPassword fail" << std::endl;
    }
	return key;
}

void Crypt::resetKeys(const Database::ptr& db) {
	salt_.clear();
	mainKey_.clear();
	srvKeyPair_.publicKey.clear();
	srvKeyPair_.secretKey.clear();
	clientKeyPairs_.clear();
	try {
		db->removeContainer("noicekeys");
		std::string baseDir = std::string(std::getenv("HOME"));
		std::filesystem::path fullPath = std::filesystem::path(baseDir) / std::string("data");
		db->remove(fullPath,"noicekeys");
	} catch (const std::filesystem::filesystem_error& e) {
		std::cerr << "Filesystem error: " << e.what() << std::endl;
	} catch (const std::runtime_error& e) {
		std::cerr << "Runtime error: " << e.what() << std::endl;
	} catch (const std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	} catch (...) {
		std::cerr << "Unknown error occurred while removing noicekeys." << std::endl;
	}
}

// 保存密钥到DB
void Crypt::saveKeys(const Database::ptr& db) {
	//save main keys
    DataContainer::ptr container = db->getContainer("noicekeys");
	if (!container) {
		container = db->addContainer("noicekeys", "collection");
	}
	auto collection = std::dynamic_pointer_cast<Collection>(container);
	Document doc0;
	std::vector<unsigned char> encryptedValue;
	
	doc0.setFieldByPath("Salt",Field(salt_));

	std::vector<unsigned char> passwordKey = deriveKeyFromPassword();

	encryptData(passwordKey, mainKey_, encryptedValue);
	doc0.setFieldByPath("mainKey",Field(encryptedValue));
	collection->insertDocument(std::hash<std::string>{}("mainkeys"), doc0);

	//save server keys
	Document doc1;
	encryptData(mainKey_, srvKeyPair_.publicKey, encryptedValue);
	doc1.setFieldByPath("publicKey",Field(encryptedValue));

	encryptData(mainKey_, srvKeyPair_.secretKey, encryptedValue);
	doc1.setFieldByPath("secretKey",Field(encryptedValue));
	//std::cout << document.toJson().dump(4);
	collection->insertDocument(std::hash<std::string>{}("serverkeys"), doc1);

	//save client keys
	Document doc2;
	for (auto& [id, keyPair] : clientKeyPairs_) {
		auto doc = std::make_shared<Document>();
		//std::vector<unsigned char> encryptedPair;
		encryptData(mainKey_, keyPair.publicKey, encryptedValue);
		doc->setFieldByPath("publicKey", Field(encryptedValue));
		//encryptedPair = encryptedValue;
		encryptData(mainKey_, keyPair.secretKey, encryptedValue);
		doc->setFieldByPath("secretKey", Field(encryptedValue));
		//encryptedPair.insert(encryptedPair.end(),encryptedValue.begin(),encryptedValue.end());
		//doc.setFieldByPath(id, Field(encryptedPair));
		doc2.setField(id, Field(doc));
	}
	collection->insertDocument(std::hash<std::string>{}("clientkeys"), doc2);
	//std::cout << doc.toJson().dump(4);
}

bool Crypt::loadClientKey(const Database::ptr& db) {
	DataContainer::ptr container = db->getContainer("noicekeys");
    if (container) {
		auto collection = std::dynamic_pointer_cast<Collection>(container);
		auto document = collection->getDocument(std::hash<std::string>{}("clientkeys"));
		if (!document) {
			std::cerr << "clientkeys not initialized\n";
			return false;
		}

		for (auto& [id, field] : document->getFields()) {
			std::vector<unsigned char> value;
			NoiseKeypair keyPair;
			auto doc = std::get<std::shared_ptr<Document>>(field.getValue());
			if (!doc) {
				std::cerr << "fail to get " << id << " pair keys \n";
			}
			Field *fld = doc->getFieldByPath("publicKey");
			if (!fld) {
				std::cerr << "fail to get " << id << ".publicKey \n";
				return false;
			}
			value = std::get<std::vector<unsigned char>>(fld->getValue());
			if (!decryptData(mainKey_, value, keyPair.publicKey)) {
				std::cerr << "fail to decrypt " << id << ".publicKey \n";
				return false;
			}
			fld = doc->getFieldByPath("secretKey");
			if (!fld) {
				std::cerr << "fail to get " << id << ".secretKey \n";
				return false;
			}
			value = std::get<std::vector<unsigned char>>(fld->getValue());
			if (!decryptData(mainKey_, value, keyPair.secretKey)) {
				std::cerr << "fail to decrypt " << id << ".secretKey \n";
				return false;
			}
			clientKeyPairs_[id] = std::move(keyPair);
		}
		return true;
	} else {
		//no client keys to load
		return true;
	}
}

bool Crypt::loadServerKey(const Database::ptr& db) {
	DataContainer::ptr container = db->getContainer("noicekeys");
    if (container) {
		auto collection = std::dynamic_pointer_cast<Collection>(container);
		auto document = collection->getDocument(std::hash<std::string>{}("serverkeys"));
		if (!document) {
			std::cerr << "serverkeys not initialized\n";
			return false;
		}
		std::vector<unsigned char> value;

		Field* field = document->getFieldByPath("publicKey");
		if (!field) {
			std::cerr << "fail to get serverkeys.publicKey \n";
			return false;
		}
		value = std::get<std::vector<unsigned char>>(field->getValue());
		if (!decryptData(mainKey_, value, srvKeyPair_.publicKey)) {
			std::cerr << "fail to decrypt serverkeys.publicKey \n";
			return false;
		}

		field = document->getFieldByPath("secretKey");
		if (!field) {
			std::cerr << "fail to get serverkeys.secretKey \n";
			return false;
		}
		value = std::get<std::vector<unsigned char>>(field->getValue());
		if (!decryptData(mainKey_, value, srvKeyPair_.secretKey)) {
			std::cerr << "fail to decrypt serverkeys.secretKey \n";
			return false;
		}

		return true;
	} else {
		//no server keys to load
		return true;
	}
}

bool Crypt::loadMainKey(const Database::ptr& db) {
	DataContainer::ptr container = db->getContainer("noicekeys");
    if (container) {
		auto collection = std::dynamic_pointer_cast<Collection>(container);
		auto document = collection->getDocument(std::hash<std::string>{}("mainkeys"));
		if (!document) {
			std::cerr << "mainkeys not initialized\n";
			return false;
		}
		std::vector<unsigned char> value;

		Field* field = document->getFieldByPath("Salt");
		if (!field) {
			std::cerr << "fail to get mainkeys.Salt \n";
			return false;
		}
		salt_ = std::get<std::vector<unsigned char>>(field->getValue());
		
		std::vector<unsigned char> passwordKey = deriveKeyFromPassword();

		field = document->getFieldByPath("mainKey");
		if (!field) {
			std::cerr << "fail to get mainkeys.mainKey \n";
			return false;
		}

		value = std::get<std::vector<unsigned char>>(field->getValue());
		if (!decryptData(passwordKey, value, mainKey_)) {
			std::cerr << "fail to decrypt mainkeys.mainKey \n";
			return false;
		}
		return true;
	} else {
		//use default keys
		return true;
	}
}


void Crypt::toJson(const std::string& filename) const{
	json jsonOutput;

    // 导出 Server Public Key
    jsonOutput["server_publicKey"] = toHexString(srvKeyPair_.publicKey);

    // 导出 Client Key Pairs（仅 Public Key）
    for (const auto& [id, keyPair] : clientKeyPairs_) {
        jsonOutput[id]["publicKey"] = toHexString(keyPair.publicKey);
		jsonOutput[id]["secretKey"] = toHexString(keyPair.secretKey);
    }

    // 写入 JSON 文件
    std::ofstream outFile(filename);
    if (!outFile) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    outFile << jsonOutput.dump(4); // 美化格式，缩进 4 空格
    outFile.close();

    std::cout << "Exported key information written to " << filename << std::endl;
}

bool Crypt::saveKeys(const std::string& client_name) {
    std::string filename = client_name + ".json"; // 生成文件名
    std::string baseDir = std::string(std::getenv("HOME"));
	std::filesystem::path fullPath = std::filesystem::path(baseDir) / std::string("etc")/ filename;
    try {
        json jsonOutput;

		// 导出 Server Public Key
		jsonOutput["server_publicKey"] = toHexString(srvKeyPair_.publicKey);

		// 导出 Client Key Pairs（仅 Public Key）
		for (const auto& [id, keyPair] : clientKeyPairs_) {
			if (client_name == id) {
				jsonOutput[id]["publicKey"] = toHexString(keyPair.publicKey);
				jsonOutput[id]["secretKey"] = toHexString(keyPair.secretKey);
				break;
			}
		}

		// 写入 JSON 文件
		std::ofstream outFile(fullPath.string());
		if (!outFile) {
			std::cerr << "Failed to open file: " << filename << std::endl;
			return false;
		}

		outFile << jsonOutput.dump(4); // 美化格式，缩进 4 空格
		outFile.close();

		std::cout << "Exported key information written to " << filename << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        return false;
    }
}

bool Crypt::loadKeys(const std::string& client_name, NoiseKeypair& keys) {
    std::string filename = client_name + ".json"; // 生成文件名
	std::string baseDir = std::string(std::getenv("HOME"));
	std::filesystem::path fullPath = std::filesystem::path(baseDir) / std::string("etc")/ filename;
    std::ifstream file(fullPath.string());
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file " << filename << std::endl;
        return false;
    }

    try {
        json json_data;
        file >> json_data; // 解析 JSON

        if (!json_data.contains(client_name)) {
            std::cerr << "Error: JSON file does not contain expected key: " << client_name << std::endl;
            return false;
        }

        // 解析公钥和私钥并转换为 vector<uint8_t>
        //keys.publicKey = hexStringToBytes(json_data[client_name]["publicKey"].get<std::string>());
        keys.secretKey = hexStringToBytes(json_data[client_name]["secretKey"].get<std::string>());
        keys.publicKey = hexStringToBytes(json_data["server_publicKey"].get<std::string>());

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        return false;
    }
}

void Crypt::showKeys() const{
	auto printVector = [](const std::string& label, const std::vector<unsigned char>& vec) {
        std::cout << label << " (size: " << vec.size() << "): ";
        for (unsigned char c : vec) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)c << " ";
        }
        std::cout << std::dec << std::endl;
    };

    std::cout << "===== Crypt Key Information =====" << std::endl;
    printVector("Salt", salt_);
    
    printVector("Main Key", mainKey_);
    printVector("Server Public Key", srvKeyPair_.publicKey);
    printVector("Server Secret Key", srvKeyPair_.secretKey);

    std::cout << "Client Key Pairs:" << std::endl;
    for (const auto& [id, keyPair] : clientKeyPairs_) {
        std::cout << "  User: " << id << std::endl;
        printVector("    Public Key", keyPair.publicKey);
        printVector("    Secret Key", keyPair.secretKey);
    }
}

}