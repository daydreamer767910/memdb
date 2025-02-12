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

bool load_user(const std::string& user, std::string& pwd) {
	DataContainer::ptr container = db->getContainer("users");
	if (!container) {
        std::cerr << "users container is missing!!\n";
		return false;
	} else {
		auto collection = std::dynamic_pointer_cast<Collection>(container);
		auto document = collection->getDocument(std::hash<std::string>{}(user));
		if (!document) {
			std::cerr << "user " << user << " is missing!!\n";
			return false;
		}
		Field* field = document->getFieldByPath("password");
		if (!field) {
			std::cerr << "fail to get password \n";
			return false;
		}
		pwd = std::get<std::string>(field->getValue());
	}
    return true;
}

void set_user(const std::string& user, const std::string& pwd, const std::string& role) {
	DataContainer::ptr container = db->getContainer("users");
	if (!container) {
		container = db->addContainer("users", "collection");
		json j = R"({
		"schema":{
			"role": {
				"type": "string",
				"constraints": {
					"required": true,
					"depth": 1
				}
			},
			"password": {
				"type": "string",
				"constraints": {
					"required": false,
					"minLength": 8,
					"maxLength": 16,
					"regexPattern": "^(?=.*[a-z])(?=.*[A-Z])(?=.*\\d)(?=.*[#@$!%*?&])[A-Za-z\\d#@$!%*?&]{8,}$"
				}
			}
		}
		})"_json;
		container->fromJson(j);
	}
    Document document;
    document.setFieldByPath(std::string("role"), Field(role));
    document.setFieldByPath(std::string("password"), Field(pwd));
    document.setFieldByPath(std::string("details.addr"), Field(std::string("12345 street, abcd, efgh")));
    document.setFieldByPath(std::string("details.info.phone"), Field(std::string("+01-123-4567-8900")));
    auto collection = std::dynamic_pointer_cast<Collection>(container);
    collection->insertDocument(std::hash<std::string>{}(user), document);
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
    std::string username,passwd;
    username = "admin";
    if (!load_user(username,passwd)) {
        std::cout << "users container is not initliazed\nplease create user for admin\n";
        // 禁用回显
        disableEcho();
        std::cout << "please enter password for admin:" << std::endl;
        std::getline(std::cin, passwd);
        // 恢复回显
        enableEcho();
        set_user(username,passwd,"admin");
        save_db();
    }
    std::cout << "please choose:\n"
          << "1. generate server keys\n"
          << "2. generate client keys\n"
          << "3. show keys\n"
          << "4. reset keys\n"
          << "5. reset password(keys will be also reset)\n"
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
    if (passwd != password) {
        std::cerr << "wrong password" << std::endl;
        return -1;
    }
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
            transport_crypt.saveKeys(userName);
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
            save_db();
            break;
        }
        case 5: {
            // 禁用回显
            disableEcho();
            std::cout << "please enter new password:" << std::endl;
            std::getline(std::cin, password);
            // 恢复回显
            enableEcho();
            set_user(username,password,"admin");
            transport_crypt.resetKeys(db);
            save_db();
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
