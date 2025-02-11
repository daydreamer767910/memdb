#include <sodium.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include "crypt.hpp"
using Transport_Crypt::Crypt;
Database::ptr& db = Database::getInstance();
Crypt transport_crypt;
void save_db() {
	std::string baseDir = std::string(std::getenv("HOME"));
	std::filesystem::path fullPath = std::filesystem::path(baseDir) / std::string("data");
//std::cout << "DBService::on_timer :save db to" << fullPath.string() << std::endl;
	db->save(fullPath.string());
}

void load_db() {
	std::string baseDir = std::string(std::getenv("HOME"));
	std::filesystem::path fullPath = std::filesystem::path(baseDir) / std::string("data");
	db->upload(fullPath.string());
}

void disableEcho() {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);    // 获取当前终端设置
    newt = oldt;                       // 复制当前设置
    newt.c_lflag &= ~ECHO;             // 关闭回显
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);  // 应用新的终端设置
}

void enableEcho() {
    struct termios oldt;
    tcgetattr(STDIN_FILENO, &oldt);    // 获取当前终端设置
    oldt.c_lflag |= ECHO;              // 启用回显
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);  // 恢复原设置
}

void test() {
    auto srvNKP = transport_crypt.getServerNKeypair();
    const Crypt::NoiseKeypair& clntNKP = transport_crypt.getClientKeypair("a").value();
    std::string str = "hello mass";
    std::vector<uint8_t> msg(str.begin(), str.end());
    std::vector<uint8_t> ciphermsg, decrymsg;
    encryptDataWithNoiseKey(clntNKP.secretKey, srvNKP.publicKey, msg, ciphermsg);
    decryptDataWithNoiseKey(clntNKP.publicKey, srvNKP.secretKey, ciphermsg, decrymsg);
    printHex(decrymsg);
    std::string decrystr(decrymsg.begin(),decrymsg.end());
    std::cout << decrystr << std::endl;
}

int main() {
    if (sodium_init() == -1) {
        std::cerr << "libsodium init fail" << std::endl;
        return -1;
    }
    load_db();
    std::cout << "please choose:\n"
          << "1. generate server keys\n"
          << "2. generate client keys\n"
          << "3. show keys\n"
          << "4. reset keys\n"
          << "5. test\n"
          << "6. to json\n";

    
    int choice;
    std::cin >> choice;
    std::cin.ignore();  // 忽略回车符
    // 禁用回显
    disableEcho();
    std::cout << "please enter password:" << std::endl;
    std::string password;
    std::getline(std::cin, password);
    // 恢复回显
    enableEcho();
    
    if (!transport_crypt.init(db, password)) return -1;
    switch (choice) {
        case 1: {
            // 生成Noise密钥对并加密存储
            auto keyPair = generateNoiseKeypair();
            transport_crypt.setServerNKeypair(keyPair);
            transport_crypt.saveKeys(db);
            
            std::cout << "server keys generated" << std::endl;
            save_db();
            break;
        }
        case 2: {
            std::cout << "please enter the userName:" << std::endl;
            std::string userName;
            std::getline(std::cin, userName);
            auto keyPair = generateNoiseKeypair();
            transport_crypt.setClientNKeypair(userName, keyPair);
            transport_crypt.saveKeys(db);
            std::cout << "user: " << userName << " keys generated" << std::endl;
            save_db();
            break;
        }
        case 3: {
            transport_crypt.showKeys();
            break;
        }
        case 4: {
            transport_crypt.resetKeys(db);
            break;
        }
        case 5: {
            test();
            break;
        }
        case 6: {
            transport_crypt.toJson("keys.json");
            break;
        }
        default:
            std::cerr << "bad choice" << std::endl;
            break;
    }
    
    return 0;
}
