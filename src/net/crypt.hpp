#ifndef CRYPT_HPP
#define CRYPT_HPP

#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include <unordered_map>

inline constexpr size_t AES_GCM_nonce_len = 12; // 标准长度，最优性能，直接用于 CTR 计数器
inline constexpr size_t AES_GCM_tag_len = 16;
inline constexpr size_t SHARED_KEY_SIZE = 32;  // X25519 共享密钥长度
inline constexpr size_t SESSION_KEY_SIZE = 32; // 每个会话密钥长度
inline constexpr unsigned int iterations_ = 3; // 迭代次数
inline constexpr size_t memory_ = 64; // 64MB 内存
inline constexpr unsigned int parallelism_ = 4; // 并行线程数

std::pair<std::vector<unsigned char>, std::vector<unsigned char>> generateKxKeypair();
std::pair<std::vector<unsigned char>, std::vector<unsigned char>> generateClientSessionKeys(
	const std::vector<unsigned char>& client_public_key,
	const std::vector<unsigned char>& client_secret_key,
	const std::vector<unsigned char>& server_public_key);
std::pair<std::vector<unsigned char>, std::vector<unsigned char>> generateServerSessionKeys(
	const std::vector<unsigned char>& server_public_key,
	const std::vector<unsigned char>& server_secret_key,
	const std::vector<unsigned char>& client_public_key);

std::vector<uint8_t> derive_key_with_password(
	const std::vector<uint8_t> &master_key, // 共享密钥（必须是 32 字节）
	uint64_t subkey_id,                     // 子密钥 ID
	const std::string &password,            // 用户密码
	size_t key_size = 32					// 需要的派生密钥长度（默认 32 bytes）
	);

void encryptData(const std::vector<unsigned char>& key, 
	const std::vector<unsigned char>& plaintext,  
	std::vector<unsigned char>& ciphertext,
	const std::vector<unsigned char>& associatedData = {});
bool decryptData(const std::vector<unsigned char>& key, 
	const std::vector<unsigned char>& ciphertext,  
	std::vector<unsigned char>& decryptedData,
	const std::vector<unsigned char>& associatedData = {});

std::vector<uint8_t> generateSalt(size_t length = 16);
std::string hashPassword(const std::string& password);
bool verifyPassword(const std::string& password, const std::string& stored_hash);

int compressData(const uint8_t* data, size_t size, std::vector<unsigned char>& compressed);
int decompressData(const std::vector<unsigned char>& compressed, uint8_t* decompressed, size_t max_size);
#endif