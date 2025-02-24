#include "crypt.hpp"
#include "util/util.hpp"
#include <argon2.h>
#include <openssl/rand.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/kdf.h>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <stdexcept>

const unsigned int iterations = 3; // 迭代次数
const size_t memory = 64; // 128MB 内存

std::pair<std::vector<unsigned char>, std::vector<unsigned char>> generateKxKeypair() {
    std::vector<unsigned char> publicKey, secretKey;
    
    // 选择椭圆曲线（如 X25519 或 P-256）
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr);
    if (!ctx || EVP_PKEY_keygen_init(ctx) <= 0) {
        std::cerr << "Error initializing key generation\n";
        return {};
    }

    // 生成密钥对
    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        std::cerr << "Error generating key pair\n";
        EVP_PKEY_CTX_free(ctx);
        return {};
    }

    EVP_PKEY_CTX_free(ctx);

    // 提取私钥
    size_t secretLen = 0;
    EVP_PKEY_get_raw_private_key(pkey, nullptr, &secretLen);
    secretKey.resize(secretLen);
    EVP_PKEY_get_raw_private_key(pkey, secretKey.data(), &secretLen);

    // 提取公钥
    size_t publicLen = 0;
    EVP_PKEY_get_raw_public_key(pkey, nullptr, &publicLen);
    publicKey.resize(publicLen);
    EVP_PKEY_get_raw_public_key(pkey, publicKey.data(), &publicLen);

    EVP_PKEY_free(pkey);  // 释放密钥

    return {publicKey, secretKey};
}

std::pair<std::vector<unsigned char>, std::vector<unsigned char>> generateSessionKeys(
    const std::vector<unsigned char>& public_key,   // 自己的公钥
    const std::vector<unsigned char>& private_key,  // 自己的私钥
    const std::vector<unsigned char>& peer_public_key, // 对方的公钥
    bool server){
    std::vector<unsigned char> shared_secret(SHARED_KEY_SIZE);

    // 创建 EVP_PKEY 结构（私钥）
    EVP_PKEY* privkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, private_key.data(), private_key.size());
    if (!privkey) {
        std::cerr << "Failed to create private key\n";
        return {};
    }

    // 创建 EVP_PKEY 结构（对方公钥）
    EVP_PKEY* pubkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr, peer_public_key.data(), peer_public_key.size());
    if (!pubkey) {
        std::cerr << "Failed to create peer public key\n";
        EVP_PKEY_free(privkey);
        return {};
    }

    // 创建 PKEY_CTX 进行密钥交换
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(privkey, nullptr);
    if (!ctx || EVP_PKEY_derive_init(ctx) <= 0 || EVP_PKEY_derive_set_peer(ctx, pubkey) <= 0) {
        std::cerr << "Key exchange init failed\n";
        EVP_PKEY_free(privkey);
        EVP_PKEY_free(pubkey);
        EVP_PKEY_CTX_free(ctx);
        return {};
    }

    // 计算共享密钥
    size_t secret_len = SHARED_KEY_SIZE;
    if (EVP_PKEY_derive(ctx, shared_secret.data(), &secret_len) <= 0 || secret_len != SHARED_KEY_SIZE) {
        std::cerr << "Key derivation failed\n";
        EVP_PKEY_free(privkey);
        EVP_PKEY_free(pubkey);
        EVP_PKEY_CTX_free(ctx);
        return {};
    }

    // 清理密钥交换上下文
    EVP_PKEY_free(privkey);
    EVP_PKEY_free(pubkey);
    EVP_PKEY_CTX_free(ctx);

    // 使用 HKDF 生成 session_rx 和 session_tx
    std::vector<unsigned char> session_keys(SESSION_KEY_SIZE * 2); // 64 字节
    EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "HKDF", nullptr);
    EVP_KDF_CTX* kdf_ctx = EVP_KDF_CTX_new(kdf);
    
    std::vector<unsigned char> hkdf_info;
    if (server) {
        hkdf_info.insert(hkdf_info.end(), public_key.begin(), public_key.end());
        hkdf_info.insert(hkdf_info.end(), peer_public_key.begin(), peer_public_key.end());
    } else {
        hkdf_info.insert(hkdf_info.end(), peer_public_key.begin(), peer_public_key.end());
        hkdf_info.insert(hkdf_info.end(), public_key.begin(), public_key.end());
    }
    
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string("digest", const_cast<char*>("SHA256"), 0),
        OSSL_PARAM_construct_octet_string("salt", nullptr, 0), // 可以使用固定盐值
        OSSL_PARAM_construct_octet_string("key", shared_secret.data(), shared_secret.size()),
        OSSL_PARAM_construct_octet_string("info", hkdf_info.data(), hkdf_info.size()),
        OSSL_PARAM_construct_end()
    };

    if (EVP_KDF_derive(kdf_ctx, session_keys.data(), session_keys.size(), params) <= 0) {
        std::cerr << "HKDF derivation failed\n";
        EVP_KDF_free(kdf);
        EVP_KDF_CTX_free(kdf_ctx);
        return {};
    }

    EVP_KDF_free(kdf);
    EVP_KDF_CTX_free(kdf_ctx);

    // 拆分 64 字节的 session_keys
    std::vector<unsigned char> session_rx(session_keys.begin(), session_keys.begin() + SESSION_KEY_SIZE);
    std::vector<unsigned char> session_tx(session_keys.begin() + SESSION_KEY_SIZE, session_keys.end());
    if (server) {
        return {session_rx, session_tx};
    } else {
        return {session_tx, session_rx};
    }
}

std::pair<std::vector<unsigned char>, std::vector<unsigned char>> generateClientSessionKeys(
	const std::vector<unsigned char>& client_public_key,
	const std::vector<unsigned char>& client_secret_key,
	const std::vector<unsigned char>& server_public_key) {
    return generateSessionKeys(client_public_key,client_secret_key,server_public_key, false);
}
std::pair<std::vector<unsigned char>, std::vector<unsigned char>> generateServerSessionKeys(
	const std::vector<unsigned char>& server_public_key,
	const std::vector<unsigned char>& server_secret_key,
	const std::vector<unsigned char>& client_public_key) {
    return generateSessionKeys(server_public_key,server_secret_key,client_public_key,true);
}

void encryptData(const std::vector<unsigned char>& key, 
    const std::vector<unsigned char>& plaintext,  
    std::vector<unsigned char>& ciphertext,
    const std::vector<unsigned char>& associatedData) {
    
    // 生成 12 字节的随机 Nonce (IV)
    std::vector<unsigned char> nonce(AES_GCM_nonce_len);
    if (!RAND_bytes(nonce.data(), nonce.size())) {
        std::cerr << "Failed to generate nonce" << std::endl;
        return;
    }
    // 计算密文所需大小 (Nonce + 密文 + 认证标签)
    ciphertext.resize(nonce.size() + plaintext.size() + AES_GCM_tag_len);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        std::cerr << "Failed to create cipher context." << std::endl;
        return;
    }

    // 初始化加密操作 (AES-256-GCM)
    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr)) {
        std::cerr << "Encryption initialization failed." << std::endl;
        EVP_CIPHER_CTX_free(ctx);
        return;
    }

    // 设置密钥和 IV
    if (1 != EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data())) {
        std::cerr << "Failed to set key and IV." << std::endl;
        EVP_CIPHER_CTX_free(ctx);
        return;
    }

    // 设置附加数据 (认证数据)
    int len;
    if (!associatedData.empty() &&
        1 != EVP_EncryptUpdate(ctx, nullptr, &len, associatedData.data(), associatedData.size())) {
        std::cerr << "Failed to set associated data." << std::endl;
        EVP_CIPHER_CTX_free(ctx);
        return;
    }

    // 执行加密操作
    int ciphertextLen = 0;
    if (1 != EVP_EncryptUpdate(ctx, ciphertext.data() + nonce.size(), &len, plaintext.data(), plaintext.size())) {
        std::cerr << "Encryption failed." << std::endl;
        EVP_CIPHER_CTX_free(ctx);
        return;
    }
    ciphertextLen = len;

    // 结束加密并获取认证标签
    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext.data() + nonce.size() + ciphertextLen, &len)) {
        std::cerr << "Failed to finalize encryption." << std::endl;
        EVP_CIPHER_CTX_free(ctx);
        return;
    }
    ciphertextLen += len;

    // 获取 GCM 认证标签并存入密文的最后 16 字节
    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, AES_GCM_tag_len, ciphertext.data() + nonce.size() + ciphertextLen)) {
        std::cerr << "Failed to get authentication tag." << std::endl;
        EVP_CIPHER_CTX_free(ctx);
        return;
    }

    // 复制 IV 到密文最前面
    std::memcpy(ciphertext.data(), nonce.data(), nonce.size());

    EVP_CIPHER_CTX_free(ctx);
}

bool decryptData(const std::vector<unsigned char>& key, 
    const std::vector<unsigned char>& ciphertext,  
    std::vector<unsigned char>& decryptedData,
    const std::vector<unsigned char>& associatedData) {

    if (ciphertext.size() < (AES_GCM_nonce_len + AES_GCM_tag_len)) { // 至少要有 Nonce (12) + Tag (16)
        std::cerr << "Ciphertext too short!" << std::endl;
        return false;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        std::cerr << "Failed to create cipher context." << std::endl;
        return false;
    }

    // 提取 Nonce
    std::vector<unsigned char> nonce(AES_GCM_nonce_len);
    std::memcpy(nonce.data(), ciphertext.data(), nonce.size());

    // 提取密文 (去掉 Nonce 和 Tag)
    size_t encryptedDataLen = ciphertext.size() - nonce.size() - AES_GCM_tag_len;
    std::vector<unsigned char> encryptedData(encryptedDataLen);
    std::memcpy(encryptedData.data(), ciphertext.data() + nonce.size(), encryptedDataLen);

    // 提取认证标签
    unsigned char tag[AES_GCM_tag_len];
    std::memcpy(tag, ciphertext.data() + nonce.size() + encryptedDataLen, AES_GCM_tag_len);

    // 初始化解密
    if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr)) {
        std::cerr << "Decryption initialization failed." << std::endl;
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    // 设置密钥和 IV
    if (1 != EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data())) {
        std::cerr << "Failed to set key and IV." << std::endl;
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    // 设置附加数据 (认证数据)
    int len;
    if (!associatedData.empty() &&
        1 != EVP_DecryptUpdate(ctx, nullptr, &len, associatedData.data(), associatedData.size())) {
        std::cerr << "Failed to set associated data." << std::endl;
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    // 执行解密
    decryptedData.resize(encryptedData.size());
    if (1 != EVP_DecryptUpdate(ctx, decryptedData.data(), &len, encryptedData.data(), encryptedData.size())) {
        std::cerr << "Decryption failed." << std::endl;
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    int plaintextLen = len;

    // 设置 GCM 认证标签
    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, AES_GCM_tag_len, tag)) {
        std::cerr << "Failed to set authentication tag." << std::endl;
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    // 结束解密
    if (1 != EVP_DecryptFinal_ex(ctx, decryptedData.data() + plaintextLen, &len)) {
        std::cerr << "Decryption failed: Authentication failed." << std::endl;
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    plaintextLen += len;

    decryptedData.resize(plaintextLen);
    EVP_CIPHER_CTX_free(ctx);
    return true;
}

std::vector<uint8_t> derive_key_with_password(
    const std::vector<uint8_t>& master_key, // 共享密钥（必须是 32 字节）
    uint64_t subkey_id,                     // 子密钥 ID
    const std::string& password,            // 用户密码
    size_t key_size) {                      // 目标密钥大小

    if (master_key.size() != 32) {  // OpenSSL 没有 crypto_kdf_KEYBYTES，直接检查 32 字节
        throw std::invalid_argument("master_key must be exactly 32 bytes long.");
    }

    // 生成 context（OpenSSL HKDF 支持长度任意的 salt，但 Libsodium 只支持 8 字节）
    std::string context = password;
    if (context.size() < 8) {
        context.resize(8, '0'); // 右侧填充 '0'
    } else {
        context = context.substr(0, 8); // 截断到 8 字节
    }

    // 目标密钥
    std::vector<uint8_t> derived_key(key_size);

    // 使用 HKDF（HMAC-SHA256）
    EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "HKDF", nullptr);
    if (!kdf) {
        throw std::runtime_error("EVP_KDF_fetch failed.");
    }

    EVP_KDF_CTX* kdf_ctx = EVP_KDF_CTX_new(kdf);
    if (!kdf_ctx) {
        EVP_KDF_free(kdf);
        throw std::runtime_error("EVP_KDF_CTX_new failed.");
    }

    // 设置 HKDF 参数
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string("digest", (char*)"SHA256", 0),
        OSSL_PARAM_construct_octet_string("salt", &subkey_id, sizeof(subkey_id)), // 8 字节 salt
        OSSL_PARAM_construct_octet_string("key", const_cast<unsigned char*>(master_key.data()), master_key.size()), // 32 字节主密钥
        OSSL_PARAM_construct_octet_string("info", context.data(), context.size()), // 8 字节 context
        OSSL_PARAM_construct_end()
    };

    // 执行 HKDF
    if (EVP_KDF_derive(kdf_ctx, derived_key.data(), derived_key.size(), params) <= 0) {
        EVP_KDF_CTX_free(kdf_ctx);
        EVP_KDF_free(kdf);
        throw std::runtime_error("HKDF derivation failed.");
    }

    // 清理资源
    EVP_KDF_CTX_free(kdf_ctx);
    EVP_KDF_free(kdf);

    return derived_key;
}

std::vector<uint8_t> generateSalt(size_t length) {
    std::vector<uint8_t> salt(length);
    if (!RAND_bytes(salt.data(), salt.size())) {
        throw std::runtime_error("Failed to generate salt");
    }
    return salt;
}

std::string hashPassword(const std::string& password) {
	size_t keyLength = 32;
    // 生成盐
    std::vector<uint8_t> salt = generateSalt();

    // 使用 Argon2 算法进行密码哈希
    std::vector<uint8_t> hash(keyLength);
    int result = argon2id_hash_raw(iterations, memory*1024, 1, password.c_str(), password.size(), salt.data(), salt.size(), hash.data(), hash.size());
    if (result != ARGON2_OK) {
        throw std::runtime_error("Argon2 hashing failed");
    }

    // 格式化输出：`salt:hash`
    std::ostringstream oss;
    oss << "$argon2id$v=19$m=" << memory << ",t=" << iterations << ",p=1$";
    for (uint8_t byte : salt) oss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    oss << "$";
    for (uint8_t byte : hash) oss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;

    return oss.str();
}

bool verifyPassword(const std::string& password, const std::string& stored_hash) {
    std::istringstream iss(stored_hash);
    // 解析头部 (argon2id)
    size_t header_end = stored_hash.find('$',1);
    if (header_end == std::string::npos || stored_hash.substr(0, header_end) != "$argon2id") {
        std::cout << "Invalid header format: " << stored_hash.substr(0, header_end) << std::endl;
        return false;
    }

    // 跳过 $argon2id$ 后的部分
    size_t pos = header_end + 1;

    // 解析版本信息 (v=19)
    size_t version_end = stored_hash.find('$', pos);
    if (version_end == std::string::npos || stored_hash.substr(pos, version_end - pos) != "v=19") {
        std::cout << "Invalid version format\n";
        return false;
    }
    pos = version_end + 1;

    // 解析参数 (m=128,t=2,p=1)
    size_t params_end = stored_hash.find('$', pos);
    if (params_end == std::string::npos) {
        std::cout << "Invalid parameters format\n";
        return false;
    }
    pos = params_end + 1;

    // 解析盐 (salt)
    size_t salt_end = stored_hash.find('$', pos);
    if (salt_end == std::string::npos) {
        std::cout << "Invalid salt format\n";
        return false;
    }
    std::string salt_str = stored_hash.substr(pos, salt_end - pos);
    pos = salt_end + 1;

    // 解析哈希值 (hash)
    std::string hash_str = stored_hash.substr(pos);


    // 确保盐字符串不是空的
    if (salt_str.empty()) {
        std::cout << "Salt string is empty!\n";
        return false;
    }

    // 解析盐
    std::vector<uint8_t> salt;
    try {
        salt = hexStringToBytes(salt_str);
    } catch (const std::exception& e) {
        std::cout << "Salt parsing error: " << e.what() << "\n";
        return false;
    }

    // 解析存储的哈希
    std::vector<uint8_t> expected_hash;
    try {
        expected_hash = hexStringToBytes(hash_str);
    } catch (const std::exception& e) {
        std::cout << "Hash parsing error: " << e.what() << "\n";
        return false;
    }

    // 计算哈希
    std::vector<uint8_t> computed_hash(expected_hash.size());
    int result = argon2id_hash_raw(iterations, memory * 1024, 1, password.c_str(), password.size(), salt.data(), salt.size(), computed_hash.data(), computed_hash.size());

    if (result != ARGON2_OK) {
        return false;
    }

    return computed_hash == expected_hash;
}

/*
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
*/