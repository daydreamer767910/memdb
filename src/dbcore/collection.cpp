#include "collection.hpp"
#include "util/util.hpp"

size_t Collection::insertDocumentsFromJson(const json& j) {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // 使用写锁，确保线程安全
    size_t inserted = 0;
    // 验证 JSON 格式
    if (!j.contains("docs")) {
        throw std::invalid_argument("Invalid JSON format: 'docs' is missing.");
    }
    if (!j["docs"].is_array()) {
        throw std::invalid_argument("Invalid JSON format: 'docs' must be an array.");
    }

    for (const auto& jDoc : j["docs"]) {
        for (const auto& [id, value] : jDoc.items()) {
            auto it = documents_.find(id);
            if (it != documents_.end()) {
                std::cout << "Document " + id + " already exist, insert fail!" << std::endl;
                continue;
            }
            try {
                auto doc = std::make_shared<Document>();
                doc->fromJson(value);  // 调用 Document 的解析方法
                documents_[id] = doc; // 插入到集合中
            } catch (const std::exception& e) {
                std::cerr << "Error parsing document with id '" << id << "': " << e.what() << std::endl;
            }
            inserted++;
        }
    }
    return inserted;
}

// 插入文档
void Collection::insertDocument(const DocumentId& id, const Document& doc) {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // 使用写锁，确保线程安全
    documents_[id] = std::make_shared<Document>(doc);
}

// 更新文档
bool Collection::updateDocument(const DocumentId& id, const Document& doc) {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // 使用写锁，确保线程安全
    auto it = documents_.find(id);
    if (it != documents_.end()) {
        it->second = std::make_shared<Document>(doc);
        return true;
    }
    return false;
}

// 删除文档
bool Collection::deleteDocument(const DocumentId& id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // 使用写锁，确保线程安全
    return documents_.erase(id) > 0;
}

// 获取文档
std::shared_ptr<Document> Collection::getDocument(const DocumentId& id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_); // 共享锁
    auto it = documents_.find(id);
    if (it != documents_.end()) {
        return it->second;
    }
    return nullptr;
}

// 查询文档
std::vector<std::shared_ptr<Document>> Collection::queryDocuments(const std::function<bool(const Document&)>& predicate) const {
    std::shared_lock<std::shared_mutex> lock(mutex_); // 共享锁
    std::vector<std::shared_ptr<Document>> result;
    for (const auto& [id, doc] : documents_) {
        if (predicate(*doc)) {
            result.push_back(doc);
        }
    }
    return result;
}

// 转换为 JSON
json Collection::toJson() const {
    std::shared_lock<std::shared_mutex> lock(mutex_); // 共享锁
    json j;
    for (const auto& [id, doc] : documents_) {
        j[id] = doc->toJson();
    }
    return j;
}

// 从 JSON 加载
void Collection::fromJson(const json& j) {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // 写锁

    // 验证 JSON 格式
    if (!j.contains("schema") ) {
        return;
    }
}

void Collection::saveSchema(const std::string& filePath) {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // 使用写锁，确保线程安全
    // 将表信息序列化为 JSON
    json root;
    root["name"] = name_;
    root["type"] = "collection";
    //other info ....

    // 将 JSON 写入文件
    std::ofstream outputFile(filePath);
    if (!outputFile.is_open()) {
        throw std::runtime_error("Unable to open file for writing: " + filePath);
    }
    outputFile << root.dump(4); // 格式化为缩进 4 的 JSON
    outputFile.close();
}

// 转换为二进制
std::string Collection::toBinary() const {
    std::shared_lock<std::shared_mutex> lock(mutex_); // 共享锁
    std::string binary;
    for (const auto& [id, doc] : documents_) {
        // 文档 ID 的长度和内容
        uint32_t idLen = id.size();
        binary.append(reinterpret_cast<const char*>(&idLen), sizeof(idLen));
        binary.append(id);

        // 文档内容的二进制
        std::string docBinary = doc->toBinary();
        uint32_t docLen = docBinary.size();
        binary.append(reinterpret_cast<const char*>(&docLen), sizeof(docLen));
        binary.append(docBinary);
    }
    return binary;
}

// 从二进制加载
void Collection::fromBinary(const char* data, size_t size) {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // 使用写锁，确保线程安全
    size_t offset = 0;
    while (offset < size) {
        // 读取文档 ID 的长度
        uint32_t idLen;
        std::memcpy(&idLen, data + offset, sizeof(idLen));
        offset += sizeof(idLen);

        // 读取文档 ID
        std::string id(data + offset, idLen);
        offset += idLen;

        // 读取文档内容的长度
        uint32_t docLen;
        std::memcpy(&docLen, data + offset, sizeof(docLen));
        offset += sizeof(docLen);

        // 读取文档内容
        std::string docBinary(data + offset, docLen);
        offset += docLen;

        // 创建并加载文档
        auto doc = std::make_shared<Document>();
        doc->fromBinary(docBinary.data(), docBinary.size());
        documents_[id] = doc;
    }
}

// 将集合导出到二进制文件
void Collection::exportToBinaryFile(const std::string& filePath) {
    // 获取集合的二进制表示
    std::string binaryData = toBinary();

    // 打开文件并写入数据
    std::ofstream outputFile(filePath, std::ios::binary);
    if (!outputFile.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filePath);
    }

    // 写入二进制数据到文件
    outputFile.write(binaryData.data(), binaryData.size());
    outputFile.close();

    std::cout << "Collection successfully exported to binary file: " << filePath << std::endl;
}

// 从二进制文件中导入集合
void Collection::importFromBinaryFile(const std::string& filePath) {
    // 打开文件并读取数据
    std::ifstream inputFile(filePath, std::ios::binary);
    if (!inputFile.is_open()) {
        throw std::runtime_error("Failed to open file for reading: " + filePath);
    }

    // 读取文件内容到字符串
    std::stringstream buffer;
    buffer << inputFile.rdbuf();
    std::string binaryData = buffer.str();
    inputFile.close();

    // 从二进制数据加载集合
    fromBinary(binaryData.data(), binaryData.size());
    std::cout << "Collection successfully imported from binary file: " << filePath << std::endl;
}
