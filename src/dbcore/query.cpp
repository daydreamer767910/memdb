#include "query.hpp"

Query& Query::condition(const std::string& path, const FieldValue& value, const std::string& op) {
	conditions.push_back({op, path, value, Field(value).getType()});
    return *this;
}
// 排序方法
Query& Query::orderBy(const std::string& path, bool ascending) {
    sorting = {path, ascending};
    return *this;
}

// 限制返回结果数量
Query& Query::limit(size_t maxResults) {
    this->maxResults = maxResults;
    return *this;
}

// 设置偏移量
Query& Query::offset(size_t startIndex) {
    this->startIndex = startIndex;
    return *this;
}

bool Query::matchCondition(const std::shared_ptr<Document>& doc, const Condition& condition) const {
    //100 K times, 130-140 ms
    auto field = doc->getFieldByPath(condition.path);
    if (!field) return false;

    const FieldValue& fieldValue = field->getValue();
    if (condition.type != field->getType()) return false;

    return std::visit([&](const auto& value) -> bool {
        return compare(value, condition.value, condition.op);
    }, fieldValue);
}

bool Query::match(std::vector<std::pair<DocumentId, std::shared_ptr<Document>>>& candidateDocs) const {
    bool isSorted = false;  // 记录最终结果是否已排序

    std::vector<size_t> indexedConditions;
    std::vector<size_t> nonIndexedConditions;

    // **1. 先遍历 conditions，分类索引和非索引条件**
    for (size_t i = 0; i < conditions.size(); ++i) {
        if (collection_.hasIndex(conditions[i].path)) {
            // **如果当前索引字段与排序字段相同，插入 indexedConditions 头部**
            if (!sorting.path.empty() && sorting.path == conditions[i].path) {
                indexedConditions.insert(indexedConditions.begin(), i);
            } else {
                indexedConditions.push_back(i);
            }
        } else {
            nonIndexedConditions.push_back(i);
        }
    }

    // **2. 先处理带索引的 conditions**
    for (size_t i = 0; i < indexedConditions.size(); ++i) {
        const auto& condition = conditions[indexedConditions[i]];
        std::vector<std::pair<DocumentId, std::shared_ptr<Document>>> filteredDocs;

        if (i == 0) { 
            // **首次使用索引，直接从索引获取初始候选集**
            auto docs = collection_.getSortedDocuments(condition.path, sorting.ascending);
            if (!sorting.path.empty() && sorting.path == condition.path) isSorted = true;  // 索引查询的结果是有序的
            for (const auto& [id, doc] : docs) {
                if (matchCondition(doc, condition)) {
                    filteredDocs.emplace_back(id, doc);
                }
            }
        } else {
            // **后续条件基于已有候选集进一步筛选**
            for (const auto& [id, doc] : candidateDocs) {
                if (matchCondition(doc, condition)) {
                    filteredDocs.emplace_back(id, doc);
                }
            }
        }

        candidateDocs = std::move(filteredDocs);

        if (candidateDocs.empty()) {
            return isSorted;  // 任何阶段筛选后为空，则返回空
        }
    }

    // **3. 处理剩余的非索引 conditions**
    for (size_t idx : nonIndexedConditions) {
        const auto& condition = conditions[idx];
        std::vector<std::pair<DocumentId, std::shared_ptr<Document>>> filteredDocs;

        if (candidateDocs.empty()) {  
            // **如果之前没有索引查询，则从 documents_ 遍历所有文档**
            for (const auto& [id, doc] : collection_.getDocuments()) {
                if (matchCondition(doc, condition)) {
                    filteredDocs.emplace_back(id, doc);
                }
            }
        } else {
            // **基于已有候选集进一步筛选**
            for (const auto& [id, doc] : candidateDocs) {
                if (matchCondition(doc, condition)) {
                    filteredDocs.emplace_back(id, doc);
                }
            }
        }

        candidateDocs = std::move(filteredDocs);

        if (candidateDocs.empty()) {
            return isSorted;  // 任何阶段筛选后为空，则返回空
        }
    }

    return isSorted;
}

void Query::page(std::vector<std::pair<DocumentId, std::shared_ptr<Document>>>& documents) {
    // 如果 startIndex 超过文档数量，则清空
    if (startIndex >= documents.size()) {
        documents.clear();
        return;
    }

    // 截取分页部分（不需要创建新 vector，直接调整原始 vector）
    documents.erase(documents.begin(), documents.begin() + startIndex);

    // 如果有最大结果限制，进行裁剪
    if (maxResults > 0 && documents.size() > maxResults) {
        documents.resize(maxResults);
    }
}

void Query::sort(std::vector<std::pair<DocumentId, std::shared_ptr<Document>>>& documents) {
    if (!sorting.path.empty()) {
        bool useIndex = collection_.hasIndex(sorting.path);  // 检查是否有索引

        if (useIndex) {
            // 直接用 map 建立 docId 到原始位置的映射，避免反复查找
            std::unordered_map<std::shared_ptr<Document>, size_t> docPos;
            for (size_t i = 0; i < documents.size(); ++i) {
                //docPos[documents[i].second] = i;
                docPos.emplace(documents[i].second, i);
            }

            // 获取按索引排序的文档
            auto sortedDocuments = collection_.getSortedDocuments(sorting.path, sorting.ascending);

            // 重新组织文档顺序
            std::vector<std::pair<DocumentId, std::shared_ptr<Document>>> sortedDocs;
            sortedDocs.reserve(documents.size());

            for (const auto& [id,doc] : sortedDocuments) {
                auto it = docPos.find(doc);
                if (it != docPos.end()) {
                    //sortedDocs.push_back(documents[it->second]);  // 保持原始 pair 结构
                    sortedDocs.emplace_back(std::move(documents[it->second]));
                }
            }

            // 更新排序后的文档
            //documents = std::move(sortedDocs);
            documents.swap(sortedDocs);
        } else {
            // 如果没有索引，执行默认的排序逻辑
            std::vector<std::pair<std::pair<DocumentId, std::shared_ptr<Document>>, std::optional<FieldValue>>> cache;
            for (const auto& docPair : documents) {
                auto field = docPair.second->getFieldByPath(sorting.path);
                std::optional<FieldValue> fieldValue;
                if (field) {
                    fieldValue = field->getValue();
                }
                cache.emplace_back(docPair, fieldValue);
            }

            std::stable_sort(cache.begin(), cache.end(), [&](const auto& a, const auto& b) {
                const auto& va = a.second;
                const auto& vb = b.second;

                if (!va) return sorting.ascending;
                if (!vb) return !sorting.ascending;

                return sorting.ascending ? (*va < *vb) : (*va > *vb);
            });

            // 重新排列 documents
            for (size_t i = 0; i < cache.size(); ++i) {
                documents[i] = cache[i].first;
            }
        }
    }
}

// 从 JSON 创建 Query 对象
Query& Query::fromJson(const json& j) {
    // 处理查询条件
    if (j.contains("conditions")) {
        for (const auto& cond : j.at("conditions")) {
            std::string path = cond.at("path").get<std::string>();
            std::string op = cond.at("op").get<std::string>();
			FieldValue value = valuefromJson(cond.at("value"));
			//std::cout << "condition: " << path << ":" << op << ":" << value;
            condition(path, value, op);
        }
		//std::cout << std::endl;
    }
    
    // 处理排序
    if (j.contains("sorting")) {
        auto sort = j.at("sorting");
        orderBy(sort.at("path").get<std::string>(), sort.at("ascending").get<bool>());
		//std::cout << "sort : " << this->sorting.path << ":" << this->sorting.ascending << "\n";
    }
    
    // 处理分页
    if (j.contains("pagination")) {
        auto pagination = j.at("pagination");
        offset(pagination.at("offset").get<size_t>());
        limit(pagination.at("limit").get<size_t>());
		//std::cout << "pageing: " << std::to_string(this->startIndex) << " : " << std::to_string(this->maxResults) << "\n";
    }

    return *this;
}