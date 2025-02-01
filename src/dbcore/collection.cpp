#include "collection.hpp"
#include "util/util.hpp"
#include "query.hpp"

std::vector<std::string> Collection::insertDocumentsFromJson(const json& j) {
    std::vector<std::string> insertedIds;
    if (!j.contains("documents")) {
        throw std::invalid_argument("Invalid JSON format: 'documents' is missing.");
    }

    std::unique_lock<std::shared_mutex> lock(mutex_);

    std::vector<std::string> failedIds;  // 用于记录失败的文档 ID
    for (const auto& jDoc : j["documents"]) {
        // 如果没有提供 ID，则生成唯一 ID
        std::string docId = (jDoc.contains("_id") && jDoc["_id"].is_string()) ? 
                            jDoc["_id"].get<std::string>() : generateUniqueId();

        auto it = documents_.find(docId);
        if (it != documents_.end()) {
            // 如果文档已存在，记录失败并继续处理下一个文档
            std::cerr << "Duplicate document ID: " << docId << std::endl;
            failedIds.push_back(docId);
            continue;
        }

        // 创建新的文档并加载 JSON 数据
        auto doc = std::make_shared<Document>();
        try {
            doc->fromJson(jDoc);  // 加载 JSON 数据到文档
            schema_.validateDocument(doc);
            
            // 尝试插入文档
            documents_.emplace(docId, std::move(doc));
            insertedIds.push_back(docId);  // 将成功插入的文档 ID 添加到返回列表
        } catch (const std::exception& e) {
            std::cerr << "Failed to insert document with ID " << docId << ": " << e.what() << std::endl;
            failedIds.push_back(docId);
        }
    }

    if (!failedIds.empty()) {
        // 可以选择在插入完成后抛出一个包含所有失败文档 ID 的异常
        std::string failedMsg = "Failed to insert the following documents: ";
        for (const auto& failedId : failedIds) {
            failedMsg += failedId + " ";
        }
        throw std::invalid_argument(failedMsg);
    }

    return insertedIds;  // 返回插入文档的 ID 列表
}

// 插入文档
void Collection::insertDocument(const DocumentId& id, const Document& doc) {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // 使用写锁，确保线程安全
    documents_[id] = std::make_shared<Document>(doc);
}

bool Collection::updateDocument(DocumentId id, const json& updateFields) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = documents_.find(id);
    if (it == documents_.end()) {
        return false; // 文档不存在
    }

    auto doc = it->second; // 获取文档
    for (auto it = updateFields.begin(); it != updateFields.end(); ++it) {
        auto& path = it.key();
        auto newValue = valuefromJson(it.value());
        //Field field = Field(valuefromJson(it.value()));
        auto field = doc->getFieldByPath(path);
        if (field) {
            field->setValue(newValue);// 更新
        } else {
            doc->setFieldByPath(path, Field(newValue));//添加新字段
        }
    }

    return true;
}

int Collection::updateFromJson(const json& j) {
    if (!j.contains("fields")) {
        throw std::invalid_argument("Update JSON must contain a 'fields' object.");
    }

    // 解析查询条件
    Query query;
    query.fromJson(j);

    // **Step 1: 解析 "fields" 并校验合法性**
    std::unordered_map<std::string, Field> parsedFields;
    const json& fieldsToUpdate = j["fields"];
    for (auto it = fieldsToUpdate.begin(); it != fieldsToUpdate.end(); ++it) {
        std::string path = it.key();
        Field field = Field(valuefromJson(it.value()));

        schema_.validateField(path, field);
        parsedFields.emplace(path, std::move(field));
    }

    std::vector<std::shared_ptr<Document>> matchedDocs;

    // **Step 2: 先筛选符合条件的文档**
    for (auto& [docId, doc] : documents_) {
        if (query.match(doc)) {
            matchedDocs.push_back(doc);
        }
    }

    // **Step 3: 获取写锁，仅更新符合条件的文档**
    std::unique_lock<std::shared_mutex> lock(mutex_);
    int updateCount = 0;

    for (auto& doc : matchedDocs) {
        bool updated = false;

        for (const auto& [path, newValue] : parsedFields) {
            auto field = doc->getFieldByPath(path);
            if (field) {
                field->setValue(newValue.getValue());
            } else {
                doc->setFieldByPath(path, newValue);
            }
            updated = true;
        }

        if (updated) {
            ++updateCount;
        }
    }

    return updateCount;
}

// 删除文档
bool Collection::deleteDocument(const DocumentId& id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // 使用写锁，确保线程安全
    return documents_.erase(id) > 0;
}

int Collection::deleteFromJson(const json& j) {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // 使用写锁，确保线程安全
    int deleteCount = 0; // 删除的文档数
    Query query;
    query.fromJson(j);

    // Step 1: 解析 "fields" 字段为集合
    std::unordered_set<std::string> deleteFields;
    if (j.contains("fields")) {
        const json& fields = j["fields"];
        for (const auto& path : fields) {
            deleteFields.insert(path.get<std::string>());
        }
    }

    // Step 2: 匹配文档并删除
    if (j.contains("conditions")) {
        for (auto it = documents_.begin(); it != documents_.end();) {
            auto& doc = it->second;
            bool hasDeletedField = false;

            // 如果文档匹配查询条件
            if (query.match(doc)) {
                // 如果有指定字段进行删除
                if (!deleteFields.empty()) {
                    for (const auto& path : deleteFields) {
                        if (doc->removeFieldByPath(path)) {
                            hasDeletedField = true;
                        } else {
                            std::cerr << "Warning: Field " << path << " does not exist in document.\n";
                        }
                    }

                    // 如果删除字段后，文档还有字段，则继续迭代
                    if (hasDeletedField && !doc->getFields().empty()) {
                        ++it; // 继续处理下一个文档
                    } else if (hasDeletedField) {
                        // 如果删除字段后，文档为空，就删除该文档
                        it = documents_.erase(it);
                        ++deleteCount;
                    } else {
                        ++it; // 如果没有字段被删除，继续迭代
                    }
                } else {
                    // 没有指定 "fields"，删除整个文档
                    it = documents_.erase(it);
                    ++deleteCount;
                }
            } else {
                ++it; // 如果没有匹配条件，继续迭代
            }
        }
    }

    return deleteCount;
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

std::vector<std::pair<DocumentId, std::shared_ptr<Document>>> Collection::queryFromJson(const json& j) const {
    std::shared_lock<std::shared_mutex> lock(mutex_); // 共享锁

    Query query;
    query.fromJson(j);
    std::vector<std::pair<DocumentId, std::shared_ptr<Document>>> results;
    
    // 遍历文档集合并执行条件匹配
    for (const auto& [docId, docPtr] : documents_) {
        if (query.match(docPtr)) {
            results.push_back({docId, docPtr});
        }
    }

    // 处理排序
    if (j.contains("sorting")) {
        query.sort(results);
    }

    // 处理分页
    if (j.contains("pagination")) {
        query.page(results);
    }

    // 如果需要投影字段，进行投影处理
    if (j.contains("fields")) {
        std::unordered_set<std::string> fieldsToProject;
        for (const auto& path : j["fields"]) {
            fieldsToProject.insert(path.get<std::string>());
        }
        std::vector<std::pair<DocumentId, std::shared_ptr<Document>>> projectedResults;
        
        // 直接通过原始文档的字段创建投影
        for (const auto& [docId, docPtr] : results) {
            auto projectedDoc = std::make_shared<Document>();
            for (const auto& path : fieldsToProject) {
                auto field = docPtr->getFieldByPath(path);
                if (field) {
                    //projectedDoc->setFieldByPath(path, *field);  // 设置投影字段
                    projectedDoc->setField(path, *field); //不需要构造文档树
                } else {
                    std::cerr << "Error: Field " << path << " does not exist in document " << docId << ".\n";
                    throw std::invalid_argument("Invalid field: " + path + " not exist in " + docId);
                }
            }
            projectedResults.push_back({docId, projectedDoc});
        }
        return projectedResults;
    }
    // 没有投影字段，直接返回查询结果
    return results;
}

json Collection::showDocs() const {
    json jsonDocs;
    { 
        // 锁定范围仅限于访问共享资源部分
        std::shared_lock<std::shared_mutex> lock(mutex_);
        //std::cout << "there are " << documents_.size() << " docs to show\n";
        for (const auto& [id, doc] : documents_) {
            jsonDocs[id] = doc->toJson();
        }
    }
    return jsonDocs;
}

// 转换为 JSON
json Collection::toJson() const {
    std::shared_lock<std::shared_mutex> lock(mutex_); // 共享锁
    json j;
    j["name"] = name_;
    j["type"] = type_;
    j["schema"] = schema_.toJson();
    return j;
}

// 从 JSON 加载
void Collection::fromJson(const json& j) {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // 写锁

    // 验证 JSON 格式
    if (!j.contains("schema")) {
        throw std::invalid_argument("Invalid JSON format: 'schema' is missing.");
    }
    try {
        schema_.fromJson(j["schema"]);
        //std::cout << schema_.toJson().dump(4) << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void Collection::saveSchema(const std::string& filePath) {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // 使用写锁，确保线程安全
    // 将表信息序列化为 JSON
    json root;
    root["name"] = name_;
    root["type"] = "collection";
    root["schema"] = schema_.toJson();

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

    //std::cout << "Collection successfully exported to binary file: " << filePath << std::endl;
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
    //std::cout << "Collection successfully imported from binary file: " << filePath << std::endl;
}
