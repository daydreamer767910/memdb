//#include <malloc.h>
#include "collection.hpp"
#include "util/util.hpp"
#include "query.hpp"

void Collection::createIndex(const std::string& path) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    //bool ret = false;
    for (const auto& [docId, docPtr] : documents_) {
        auto field = docPtr->getFieldByPath(path);
        if (field) {
            //std::cout << path << " -> value: " << *field << " -> docID: " << docId << std::endl;
            indexedFields_[path][field->getValue()].insert(docId); // 存储字段值
            //ret = true;
        } else {
            // 可以选择不插入空字段的索引，或者标记为空值
            indexedFields_[path][std::monostate{}].insert(docId);
        }
    }
    // 打印索引内容，查看空值排列情况
    /*std::cout << "Index for path: " << path << std::endl;
    for (const auto& [value, docIds] : indexedFields_[path]) {
        std::cout << "Value: ";
        std::visit([](const auto& v) {
            if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::monostate>) {
                std::cout << "null";
            } else {
                std::cout << v;
            }
        }, value);
        std::cout << " -> Doc IDs(" << docIds.size() << "): ";
        for (const auto& docId : docIds) {
            std::cout << docId << " ";
        }
        std::cout << std::endl;
    }*/
}

void Collection::updateIndex(const std::string& path, const DocumentId& docId, const FieldValue& newValue) {
    auto indexIt = indexedFields_.find(path);
    if (indexIt == indexedFields_.end()) return; // 若索引不存在，直接返回

    auto& fieldMap = indexIt->second;  // 获取当前字段的索引映射

    // 先找到旧值并删除
    for (auto it = fieldMap.begin(); it != fieldMap.end(); ++it) {
        auto& docSet = it->second;
        if (docSet.erase(docId) > 0) {  
            if (docSet.empty()) {  // 如果该值下没有文档了，移除该值
                fieldMap.erase(it);
            }
            break; // 找到并删除后，退出循环
        }
    }

    // 插入新值
    // 使用 try_emplace 避免重复查找
    auto& docSet = fieldMap.try_emplace(newValue).first->second;
    docSet.insert(docId);
}

// **更新索引删除字段**
void Collection::deleteIndex(const std::string& path, const DocumentId& docId, const FieldValue& deleteValue) {
    // 查找索引中的条目
    auto indexIt = indexedFields_.find(path);
    if (indexIt != indexedFields_.end()) {
        auto& valueMap = indexIt->second;

        // 查找该字段值是否存在于索引中
        auto valueIt = valueMap.find(deleteValue);
        if (valueIt != valueMap.end()) {
            auto& docSet = valueIt->second;
            //std::cout << "erase idx from: k[" << path << "]->v[" << valueToDelete << "]->doc[" << docId << "]\n";
            // 从索引中删除该文档
            docSet.erase(docId);
            // 如果该字段值在索引中没有文档引用，删除该索引条目
            if (docSet.empty()) {
                //std::cout << "also erase set for key: " << valueToDelete << "\n";
                valueMap.erase(valueIt);
            }
        }
    } else {
        std::cerr << "Path not found in indexedFields: " << path << std::endl;
    }
}

void Collection::deleteIndex(const DocumentId& docId) {
    auto it = documents_.find(docId);
    if (it == documents_.end()) {
        std::cerr << "document not found in document_: " << docId << std::endl;
        return; // 文档不存在
    }
    auto doc = it->second; // 获取文档 
    // 从索引中删除
    for (const auto& [fieldPath, field] : doc->getFields()) {
        auto fieldIt = indexedFields_.find(fieldPath);
        if (fieldIt != indexedFields_.end()) {
            auto valueIt = fieldIt->second.find(field.getValue());
            if (valueIt != fieldIt->second.end()) {
                valueIt->second.erase(docId);
                if (valueIt->second.empty()) {
                    fieldIt->second.erase(valueIt);
                }
            }
        }
    }
}

void Collection::dropIndex(const std::string& path) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = indexedFields_.find(path);
    if (it != indexedFields_.end()) {
        std::map<FieldValue, std::unordered_set<DocumentId>>().swap(it->second);  // 释放内存
        indexedFields_.erase(it);
        //malloc_trim(0);
    }
}

// 根据字段路径获取排序后的文档列表
std::vector<std::pair<DocumentId, FieldValue>> Collection::getSortedDocuments(const std::string& path,
    const std::vector<DocumentId>& candidateDocs) const {
    std::vector<std::pair<DocumentId, FieldValue>> sortedDocs;
    auto indexIt = indexedFields_.find(path);
    
    if (indexIt == indexedFields_.end()) {
        return sortedDocs; // 索引不存在，返回空结果
    }

    const auto& valueMap = indexIt->second;  // 直接引用，减少查找
    if (candidateDocs.empty()) {
        // **没有候选文档，直接返回所有索引中的文档**
        size_t estimatedSize = 0;
        
        // 计算需要存储的文档数量，减少后续 push_back 的扩容
        for (const auto& [_, docSet] : valueMap) {
            estimatedSize += docSet.size();
        }
        sortedDocs.reserve(estimatedSize);

        // 遍历所有字段值对应的文档集
        for (const auto& [fieldValue, docSet] : valueMap) {
            for (const auto& docId : docSet) {
                sortedDocs.emplace_back(docId, fieldValue);
            }
        }
    } else {
        // **有候选文档，只保留 candidateDocs 里存在于索引的文档**
        std::unordered_set<DocumentId> candidateSet(candidateDocs.begin(), candidateDocs.end());
        sortedDocs.reserve(candidateSet.size());

        for (const auto& [fieldValue, docSet] : valueMap) {
            for (const auto& docId : docSet) {
                if (candidateSet.find(docId) != candidateSet.end()) {
                    sortedDocs.emplace_back(docId, fieldValue);
                }
            }
        }
    }
    //std::cout << "size of sortedDocs: " << sortedDocs.size() <<  std::endl;
    return sortedDocs;
}

std::vector<DocumentId> Collection::insertDocumentsFromJson(const json& j) {
    std::vector<DocumentId> insertedIds;
    if (!j.contains("documents")) {
        throw std::invalid_argument("Invalid JSON format: 'documents' is missing.");
    }

    std::unique_lock<std::shared_mutex> lock(mutex_);

    std::vector<DocumentId> failedIds;  // 用于记录失败的文档 ID
    for (const auto& jDoc : j["documents"]) {
        // 如果没有提供 ID，则生成唯一 ID
        DocumentId docId;
        if (jDoc.contains("_id") && jDoc["_id"].is_number_integer()) {
            docId = jDoc["_id"].get<DocumentId>();
        } else {
            docId = std::hash<std::string>{}(generateUniqueId());
        }
        
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
            
            // 插入文档
            documents_.emplace(docId, doc);
            insertedIds.push_back(docId);
        } catch (const std::exception& e) {
            std::cerr << "Failed to insert document with ID " << docId << ": " << e.what() << std::endl;
            failedIds.push_back(docId);
        }
    }
    //批量更新索引
    for (const auto& docId: insertedIds ) {
        auto doc = this->getDocumentNoLock(docId);
        // **更新索引**
        for (const auto& [path, field] : doc->getFields()) {  
            updateIndex(path, docId, field.getValue());
        }
    }

    if (!failedIds.empty()) {
        // 可以选择在插入完成后抛出一个包含所有失败文档 ID 的异常
        std::string failedMsg = "Failed to insert the following documents: ";
        for (const auto& failedId : failedIds) {
            failedMsg += std::to_string(failedId) + " ";
        }
        throw std::invalid_argument(failedMsg);
    }

    return insertedIds;  // 返回插入文档的 ID 列表
}

// 插入文档
void Collection::insertDocument(const DocumentId& id, const Document& doc) {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // 使用写锁，确保线程安全
    
    try {
        auto docPtr = std::make_shared<Document>(doc);
        schema_.validateDocument(docPtr);
        documents_[id] = docPtr;
        // **更新索引**
        for (const auto& [path, field] : doc.getFields()) {  
            updateIndex(path, id, field.getValue());
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to insert document with ID " << id << ": " << e.what() << std::endl;
    }
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
        updateIndex(path, id, newValue);
    }

    return true;
}

int Collection::updateFromJson(const json& j) {
    if (!j.contains("fields")) {
        throw std::invalid_argument("Update JSON must contain a 'fields' object.");
    }

    // 解析查询条件
    Query query(*this);
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
    // **Step 3: 获取写锁，仅更新符合条件的文档**
    std::unique_lock<std::shared_mutex> lock(mutex_);
    std::vector<DocumentId> matchedDocs;
    query.match(matchedDocs);
    
    int updateCount = 0;

    for (auto& id : matchedDocs) {
        bool updated = false;
        auto doc = this->getDocumentNoLock(id);
        if (!doc) continue;
        for (const auto& [path, newValue] : parsedFields) {
            auto field = doc->getFieldByPath(path);
            if (field) {
                field->setValue(newValue.getValue());
            } else {
                doc->setFieldByPath(path, newValue);
            }
            updated = true;
            updateIndex(path, id, newValue.getValue());
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
    int deleteCount = 0; // 删除的文档数
    Query query(*this);
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
    std::unique_lock<std::shared_mutex> lock(mutex_);
    std::vector<DocumentId> matchedDocs;
    query.match(matchedDocs);

    for (auto& id: matchedDocs) {
        auto doc = this->getDocumentNoLock(id);
        if (!doc) continue;
        bool hasDeletedField = false;
        // 如果有指定字段进行删除
        if (!deleteFields.empty()) {
            for (const auto& path : deleteFields) {
                Field removedField = doc->removeFieldByPath(path);

                hasDeletedField = true;
                updateIndex(path, id, std::monostate{});
                ++deleteCount;
            }

            // 如果删除字段后，文档为空，就删除该文档
            if (hasDeletedField && doc->getFields().empty()) {
                // **删除文档时，也要从索引中清除该文档**
                deleteIndex(id);
                // 如果删除字段后，文档为空，就删除该文档
                documents_.erase(id);
            }
        } else {
            // **删除文档时，也要从索引中清除该文档**
            deleteIndex(id);
            // 没有指定 "fields"，删除整个文档
            documents_.erase(id);
            ++deleteCount;
        }

    }
    
    return deleteCount;
}

std::shared_ptr<Document> Collection::getDocumentNoLock(const DocumentId& id) const {
    auto it = documents_.find(id);
    if (it != documents_.end()) {
        return it->second;
    }
    return nullptr;
}

// 获取文档
std::shared_ptr<Document> Collection::getDocument(const DocumentId& id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);  // 共享锁
    return getDocumentNoLock(id);
}

std::vector<std::pair<DocumentId, std::shared_ptr<Document>>> Collection::queryFromJson(const json& j) const {
    std::shared_lock<std::shared_mutex> lock(mutex_); // 共享锁
    Query query(*this);
    query.fromJson(j);

    std::vector<DocumentId> results;
    if (j.contains("conditions") && j.at("conditions").size() > 0) {
        query.match(results);
    } else {
        for (const auto& docPair : documents_) {
            results.emplace_back(docPair.first);
        }
        query.sort(results);
    }
    
    // 处理分页
    if (j.contains("pagination")) {
        query.page(results);
    }

    // 如果需要投影字段，进行投影处理
    if (j.contains("fields")) {
        std::vector<std::string> fieldsToProject;
        for (const auto& path : j["fields"]) {
            fieldsToProject.push_back(path.get<std::string>());
        }

        std::vector<std::pair<DocumentId, std::shared_ptr<Document>>> projectedResults;
        
        // 直接通过原始文档的字段创建投影
        for (const auto& docId : results) {
            auto docPtr = this->getDocumentNoLock(docId);
            if (!docPtr) continue;
            auto projectedDoc = std::make_shared<Document>();
            for (const auto& path : fieldsToProject) {
                auto field = docPtr->getFieldByPath(path);
                if (field) {
                    //projectedDoc->setFieldByPath(path, *field);  // 设置投影字段
                    projectedDoc->setField(path, *field); //不需要构造文档树
                } else {
                    std::cerr << "Error: Field " << path << " does not exist in document " << std::to_string(docId) << ".\n";
                    throw std::invalid_argument("Invalid field: " + path + " not exist in " + std::to_string(docId));
                }
            }
            projectedResults.push_back({docId, projectedDoc});
        }
        return projectedResults;
    } else {
        std::vector<std::pair<DocumentId, std::shared_ptr<Document>>> ret;
        for (const auto& docId : results) {
            auto docPtr = this->getDocumentNoLock(docId);
            if (!docPtr) continue;
            ret.emplace_back(docId,docPtr);
        }
        return ret;
    }
}

void Collection::showDocs() const {
    { 
        // 锁定范围仅限于访问共享资源部分
        std::shared_lock<std::shared_mutex> lock(mutex_);
        //std::cout << "there are " << documents_.size() << " docs to show\n";
        for (const auto& [id, doc] : documents_) {
            std::cout << id << " : \n";
            std::cout << doc->toJson().dump(4) << "\n";
        }
    }
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
        schema_.fromJson(j);
        //std::cout << schema_.toJson().dump(4) << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void Collection::saveSchema(const std::string& filePath) {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // 使用写锁，确保线程安全
    // 将表信息序列化为 JSON
    json root = schema_.toJson();
    root["name"] = name_;
    root["type"] = "collection";

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
        // 直接使用 uint64_t 类型的 ID，转换为字节流
        binary.append(reinterpret_cast<const char*>(&id), sizeof(id));

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
        // 读取文档 ID
        DocumentId id;
        std::memcpy(&id, data + offset, sizeof(id));  // 直接读取 uint64_t 类型的 ID
        offset += sizeof(id);

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
