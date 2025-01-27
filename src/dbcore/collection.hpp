#ifndef COLLECTION_HPP
#define COLLECTION_HPP
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <stdexcept>
#include "datacontainer.hpp"
#include "document.hpp"
#include "collection_schema.hpp"
#include "query.hpp"

class Collection: public DataContainer {
public:
    using DocumentId = std::string;
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

    std::vector<std::string> insertDocumentsFromJson(const json& j);
    // 添加一个文档
    void insertDocument(const DocumentId& id, const Document& doc);

    // 单文档更新
    bool updateDocument(DocumentId id, const json& updateFields);

    // 根据查询条件批量更新文档
    int updateFromJson(const json& j);

    // 删除一个文档（通过 ID）
    bool deleteDocument(const DocumentId& id);

    // 根据 ID 获取文档
    std::shared_ptr<Document> getDocument(const DocumentId& id) const;

    // 查询文档集合，支持过滤
    std::vector<std::shared_ptr<Document>> queryFromJson(const json& j) const;

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
};


#endif 
