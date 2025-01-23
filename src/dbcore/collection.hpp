#ifndef COLLECTION_HPP
#define COLLECTION_HPP
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>
#include <stdexcept>

class Document; // 预声明

class Collection {
public:
    // 用于存储Document的ID和对应的Document对象
    std::unordered_map<std::string, std::shared_ptr<Document>> documents;

    // 插入文档
    void insert(const std::shared_ptr<Document>& doc) {
        // 假设文档有一个唯一的ID，检查是否已存在
        if (documents.find(doc->getId()) != documents.end()) {
            throw std::runtime_error("Document with this ID already exists.");
        }
        documents[doc->getId()] = doc;
    }

    // 删除文档
    void remove(const std::string& docId) {
        auto it = documents.find(docId);
        if (it != documents.end()) {
            documents.erase(it);
        } else {
            throw std::runtime_error("Document not found.");
        }
    }

    // 查找文档
    std::shared_ptr<Document> find(const std::string& docId) {
        auto it = documents.find(docId);
        if (it != documents.end()) {
            return it->second;
        }
        throw std::runtime_error("Document not found.");
    }

    // 更新文档
    void update(const std::string& docId, const std::shared_ptr<Document>& newDoc) {
        auto it = documents.find(docId);
        if (it != documents.end()) {
            documents[docId] = newDoc;
        } else {
            throw std::runtime_error("Document not found.");
        }
    }

    // 获取所有文档
    std::vector<std::shared_ptr<Document>> getAllDocuments() const {
        std::vector<std::shared_ptr<Document>> allDocs;
        for (const auto& docPair : documents) {
            allDocs.push_back(docPair.second);
        }
        return allDocs;
    }
};

#endif 
