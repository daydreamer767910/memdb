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
#include "dbcore/database.hpp"

std::pair<std::vector<unsigned char>, std::vector<unsigned char>> generateNoiseKeypair();
void encryptData(const std::vector<unsigned char>& key, 
	const std::vector<unsigned char>& data, 
	std::vector<unsigned char>& ciphertext);
bool decryptData(const std::vector<unsigned char>& key, 
	const std::vector<unsigned char>& ciphertext, 
	std::vector<unsigned char>& decryptedData);
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

#endif