#ifndef COLLECTION_HPP
#define COLLECTION_HPP
#include <unordered_map>
#include <string>
#include <unordered_set>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <stdexcept>
#include "datacontainer.hpp"
#include "document.hpp"
#include "collection_schema.hpp"


class Collection: public DataContainer {
public:
    
    explicit Collection(const std::string& name, const std::string& type) : DataContainer(name,type) {}
    //Table(const std::string& tableName, const std::vector<Column>& columns);
    // Delete copy constructor and copy assignment operator
    Collection(const Collection&) = delete;
    Collection& operator=(const Collection&) = delete;

    // Default constructor (move semantics can be allowed)
    Collection() = default;

    // Move constructor and move assignment operator (optional, if needed)
    Collection(Collection&&) = default;
    Collection& operator=(Collection&&) = default;

    size_t getTotalDocument() {
        return documents_.size();
    }
    
    std::vector<std::pair<std::string, std::shared_ptr<Document>>> getDocuments() const {
        std::vector<std::pair<std::string, std::shared_ptr<Document>>> documentsVec;
        documentsVec.reserve(documents_.size());  // 预分配空间
        for (const auto& pair : documents_) {
            documentsVec.emplace_back(pair.first, pair.second);
        }
        return documentsVec;
    }


    std::vector<std::string> insertDocumentsFromJson(const json& j);
    // 添加一个文档
    void insertDocument(const DocumentId& id, const Document& doc);

    // 单文档更新
    bool updateDocument(DocumentId id, const json& updateFields);

    // 根据查询条件批量更新文档
    int updateFromJson(const json& j);

    // 删除一个文档（通过 ID）
    bool deleteDocument(const DocumentId& id);

    int deleteFromJson(const json& j);

    // 根据 ID 获取文档
    std::shared_ptr<Document> getDocument(const DocumentId& id) const;

    // 查询文档集合，支持过滤
    std::vector<std::pair<DocumentId, std::shared_ptr<Document>>> queryFromJson(const json& j) const;

    // 创建索引：为指定字段创建索引
    void createIndex(const std::string& path);

    void updateIndex(const std::string& path, const DocumentId& docId, const FieldValue& newValue);
    // 删除索引
    void removeIndex(const std::string& path);
    void updateIndexForDeletedField(const std::string& path, const DocumentId& docId);
    void updateIndexForDeletedDoc(const DocumentId& docId);

    // 检查是否有索引
    bool hasIndex(const std::string& path) const {
        return indexedFields_.find(path) != indexedFields_.end();
    }

    std::vector<std::pair<DocumentId, std::shared_ptr<Document>>> getSortedDocuments(const std::string& path) const;
    // 序列化和反序列化
    virtual json toJson() const override;
    virtual void fromJson(const json& j) override;
    json showDocs() const;
    virtual void saveSchema(const std::string& filePath) override;
    virtual void exportToBinaryFile(const std::string& filePath) override;
    virtual void importFromBinaryFile(const std::string& filePath) override;

    std::string toBinary() const;
    void fromBinary(const char* data, size_t size);
    
private:
    std::unordered_map<DocumentId, std::shared_ptr<Document>> documents_;
    CollectionSchema schema_;
    // 索引映射：用于存储字段路径 -> 字段值 -> 文档ID
    std::unordered_map<std::string, std::map<FieldValue, std::set<DocumentId>>> indexedFields_;
};

#endif 
