#ifndef CRYPT_HPP
#define CRYPT_HPP

#include <sodium.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include <unordered_map>


std::pair<std::vector<unsigned char>, std::vector<unsigned char>> generateKxKeypair();
std::pair<std::vector<unsigned char>, std::vector<unsigned char>> generateClientSessionKeys(
	const std::vector<unsigned char>& client_public_key,
	const std::vector<unsigned char>& client_secret_key,
	const std::vector<unsigned char>& server_public_key);
std::pair<std::vector<unsigned char>, std::vector<unsigned char>> generateServerSessionKeys(
	const std::vector<unsigned char>& server_public_key,
	const std::vector<unsigned char>& server_secret_key,
	const std::vector<unsigned char>& client_public_key);
std::vector<uint8_t> derive_key_with_argon2(
		const std::vector<uint8_t> &shared_key, // ECDH 共享密钥
		const std::string &password,           // 用户密码
		const std::vector<uint8_t> &salt,      // 随机 Salt
		size_t key_size = 32                   // 需要的派生密钥长度（默认 32 bytes）
	);
std::vector<uint8_t> derive_key_with_password(
	const std::vector<uint8_t> &master_key, // 共享密钥（必须是 32 字节）
	uint64_t subkey_id,                     // 子密钥 ID
	const std::string &password,            // 用户密码
	size_t key_size = 32					// 需要的派生密钥长度（默认 32 bytes）
	);
std::pair<std::vector<unsigned char>, std::vector<unsigned char>> generateNoiseKeypair();
void encryptData(const std::vector<unsigned char>& key, 
	const std::vector<unsigned char>& plaintext,  
	std::vector<unsigned char>& ciphertext,
	const std::vector<unsigned char>& associatedData = {});
bool decryptData(const std::vector<unsigned char>& key, 
	const std::vector<unsigned char>& ciphertext,  
	std::vector<unsigned char>& decryptedData,
	const std::vector<unsigned char>& associatedData = {});
void encryptDataWithNoiseKey(
	const std::vector<unsigned char>& senderSecretKey,
	const std::vector<unsigned char>& receiverPublicKey,
	const std::vector<unsigned char>& plaintext,
	std::vector<unsigned char>& ciphertext);
bool decryptDataWithNoiseKey(
	const std::vector<unsigned char>& senderPublicKey,
	const std::vector<unsigned char>& receiverSecretKey,
	const std::vector<unsigned char>& ciphertext,
	std::vector<unsigned char>& decryptedData);
std::string hashPassword(const std::string& password);
bool verifyPassword(const std::string& password, const std::string& stored_hash);

/*
namespace Transport_Crypt {
class Crypt {
public:
	struct NoiseKeypair {
		std::vector<unsigned char> publicKey{std::vector<unsigned char>(crypto_box_PUBLICKEYBYTES)};
    	std::vector<unsigned char> secretKey{std::vector<unsigned char>(crypto_box_SECRETKEYBYTES)};
	};
	bool init(const Database::ptr& db, const std::string& password = "memdb");
	
	void setServerNKeypair(const std::pair<std::vector<unsigned char>, std::vector<unsigned char>>& keyPair) {
		srvKeyPair_.publicKey = keyPair.first;
		srvKeyPair_.secretKey = keyPair.second;
	}

	const NoiseKeypair& getServerNKeypair() const {
		return srvKeyPair_;
	}

	void setClientNKeypair(const std::string& clientId, const std::pair<std::vector<unsigned char>, std::vector<unsigned char>>& keyPair) {
		NoiseKeypair keypair;
		keypair.publicKey = keyPair.first;
		keypair.secretKey = keyPair.second;
		clientKeyPairs_[clientId] = keypair;
	}

	const std::optional<std::reference_wrapper<const NoiseKeypair>> getClientKeypair(const std::string& clientId) const {
		auto it = clientKeyPairs_.find(clientId);
		if (it != clientKeyPairs_.end()) {
			return it->second;
		}
		return std::nullopt; // 返回空值
	}	
	void showKeys() const;
	void saveKeys(const Database::ptr& db);
	void resetKeys(const Database::ptr& db);
	void toJson(const std::string& filename) const;
	bool saveKeys(const std::string& client_name);
	bool loadKeys(const std::string& client_name, NoiseKeypair& keys);
private:
	std::vector<unsigned char> deriveKeyFromPassword();
	bool loadMainKey(const Database::ptr& db);
	bool loadServerKey(const Database::ptr& db);
	bool loadClientKey(const Database::ptr& db);
private:
	std::string password_;
	std::vector<unsigned char> salt_;
	std::vector<unsigned char> mainKey_;
	NoiseKeypair srvKeyPair_;
	std::unordered_map<std::string, NoiseKeypair> clientKeyPairs_;
};
}
*/
#endif