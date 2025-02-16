#include <sodium.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include "crypt.hpp"
#include "dbcore/database.hpp"

Database::ptr& db = Database::getInstance();

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

void show_users() {
	DataContainer::ptr container = db->getContainer("users");
	if (container) {
		auto collection = std::dynamic_pointer_cast<Collection>(container);
        collection->showDocs();
	}
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
            "name": {
				"type": "string",
				"constraints": {
					"required": true,
                    "minLength": 3,
                    "maxLength": 32,
					"depth": 1
				}
			},
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
					"required": true,
                    "depth": 1
				}
			}
		}
		})"_json;
		container->fromJson(j);
	}
    Document document;
    document.setFieldByPath(std::string("name"), Field(user));
    document.setFieldByPath(std::string("role"), Field(role));
    document.setFieldByPath(std::string("password"), Field(pwd));
    document.setFieldByPath(std::string("details.addr"), Field(std::string("12345 street, abcd, efgh")));
    document.setFieldByPath(std::string("details.info.phone"), Field(std::string("+01-123-4567-8900")));
    auto collection = std::dynamic_pointer_cast<Collection>(container);
    collection->insertDocument(std::hash<std::string>{}(user), document);
}

void del_user(const std::string& user) {
	DataContainer::ptr container = db->getContainer("users");
	if (container) {
		auto collection = std::dynamic_pointer_cast<Collection>(container);
        collection->deleteDocument(std::hash<std::string>{}(user));
	}
}

void show_user(const std::string& user) {
	DataContainer::ptr container = db->getContainer("users");
	if (container) {
		auto collection = std::dynamic_pointer_cast<Collection>(container);
        auto doc = collection->getDocument(std::hash<std::string>{}(user));
        if (doc) {
            
            std::cout << doc->toJson().dump(4) << std::endl;
        } else {
            std::cout << "user not exist\n";
        }
	}
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
/*
void test() {
    Crypt transport_crypt;
    transport_crypt.init(db,"fuckyou");
    auto srvNKP = transport_crypt.getServerNKeypair();
    const Crypt::NoiseKeypair& clntNKP = transport_crypt.getClientKeypair("memdb").value();
    std::string str = "hello mass";
    std::vector<uint8_t> msg(str.begin(), str.end());
    std::vector<uint8_t> ciphermsg, decrymsg;
    encryptDataWithNoiseKey(clntNKP.secretKey, srvNKP.publicKey, msg, ciphermsg);
    decryptDataWithNoiseKey(clntNKP.publicKey, srvNKP.secretKey, ciphermsg, decrymsg);
    printHex(decrymsg);
    std::string decrystr(decrymsg.begin(),decrymsg.end());
    std::cout << decrystr << std::endl;
}*/

void testkx() {
    auto clientKxPair = generateKxKeypair();
    std::cout << "Client Public Key:\n";
    printHex(clientKxPair.first);
    std::cout << "Client Secret Key:\n";
    printHex(clientKxPair.second);

    auto serverKxPair = generateKxKeypair();
    std::cout << "Server Public Key:\n";
    printHex(serverKxPair.first);
    std::cout << "Server Secret Key:\n";
    printHex(serverKxPair.second);

    auto sessionKc = generateClientSessionKeys(clientKxPair.first, clientKxPair.second, serverKxPair.first);
    auto sessionKs = generateServerSessionKeys(serverKxPair.first, serverKxPair.second, clientKxPair.first);

    std::cout << "shared Key (Client Side):\n";
    printHex(sessionKc.first);
    printHex(sessionKc.second);
    std::cout << "shared Key (Server Side):\n";
    printHex(sessionKs.first);
    printHex(sessionKs.second);
    std::string pwd = "oumass";
    std::vector<uint8_t> salt(crypto_pwhash_SALTBYTES);
	randombytes_buf(salt.data(), salt.size());  // 生成随机 salt
    auto sessionK_rx = derive_key_with_argon2(sessionKc.first, pwd, salt);
    auto sessionK_tx = derive_key_with_argon2(sessionKc.second, pwd, salt);
    std::cout << "session Key (Client Side):\n";
    printHex(sessionK_rx);
    printHex(sessionK_tx);
    sessionK_rx = derive_key_with_argon2(sessionKs.first, pwd, salt);
    sessionK_tx = derive_key_with_argon2(sessionKs.second, pwd, salt);
    std::cout << "session Key (Server Side):\n";
    printHex(sessionK_rx);
    printHex(sessionK_tx);

    std::string str = "hello mass";
    std::vector<uint8_t> msg(str.begin(), str.end());
    std::vector<uint8_t> ciphermsg, decrymsg;
    std::string associatedStr = std::to_string(time(nullptr));
    std::vector<uint8_t> associatedData(associatedStr.c_str(), associatedStr.c_str()+associatedStr.size());
    encryptData(sessionK_rx, msg, ciphermsg);
    printHex(ciphermsg);
    decryptData(sessionK_rx, ciphermsg, decrymsg);
    std::string decrystr(decrymsg.begin(),decrymsg.end());
    std::cout << decrystr << std::endl;
}

int main() {
    if (sodium_init() == -1) {
        std::cerr << "libsodium init fail" << std::endl;
        return -1;
    }
    load_db();
    //testkx();
    //test();
    int choice;
    std::string username, passwd, hashedPwd, role;
    
    if (!load_user("admin",hashedPwd)) {
        std::cout << "users admin is not initliazed\nplease create user for admin\n";
        choice = 1;
        username = "admin";
        role = "admin";
    } else {
        std::cout << "please choose:\n"
            << "1. create user\n"
            << "2. delete user\n"
            << "3. show user\n"
            << "4. show all users\n";
            
        std::cin >> choice;
        std::cin.ignore();  // 忽略回车符
        // 禁用回显
        disableEcho();
        std::cout << "please enter password for admin:" << std::endl;
        std::getline(std::cin, passwd);
        // 恢复回显
        enableEcho();
        
        if (!verifyPassword(passwd, hashedPwd)) {
            std::cerr << "wrong password" << std::endl;
            return -1;
        }
    }
    
    switch (choice) {
        case 1: {
            if (username.empty()) {
                std::cout << "please enter new username:" << std::endl;
                std::getline(std::cin, username);
                std::cout << "please enter the role(admin/user/guest):" << std::endl;
                std::getline(std::cin, role);
            }
            
            while (1) {
                std::string cofirmPwd;
                // 禁用回显
                disableEcho();
                std::cout << "please enter new password for: " << username << std::endl;
                std::getline(std::cin, passwd);
                std::cout << "please confirm new password:" << std::endl;
                std::getline(std::cin, cofirmPwd);
                // 恢复回显
                enableEcho();
                if (passwd == cofirmPwd)
                    break;
                else
                    std::cout << "passwords not match\n";
            }
            hashedPwd = hashPassword(passwd);
            //std::cout << "pwd: " << hashedPwd << std::endl;
            set_user(username,hashedPwd,role);
            //std::cout << "user: " << username << " generated" << std::endl;
            save_db();
            break;
        }
        case 2: {
            std::cout << "please enter the userName to be deleted:" << std::endl;
            std::getline(std::cin, username);
            if (!load_user(username,hashedPwd)) {
                std::cerr << "users " << username << " not exist\n";
                return -1;
            }
            del_user(username);
            
            save_db();
            break;
        }
        case 3: {
            std::cout << "please enter the userName to show:" << std::endl;
            std::getline(std::cin, username);
            if (!load_user(username,hashedPwd)) {
                std::cerr << "users " << username << " not exist\n";
                return -1;
            }
            show_user(username);
            break;
        }
        case 4: {
            show_users();
            break;
        }
        default:
            std::cerr << "bad choice" << std::endl;
            break;
    }
    
    return 0;
}
