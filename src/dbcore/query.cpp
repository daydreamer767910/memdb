#include "query.hpp"

Query& Query::condition(const std::string& path, const FieldValue& value, const std::string& op) {
	conditions.push_back({op, path, value, getValueType(value)});
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
    auto defaultV = Field(getDefault(condition.type));
    auto field = doc->getFieldByPath(condition.path);
    if (!field) {
        field = &defaultV;
    }
    // 针对 LIKE 操作符的特殊处理
    if (condition.op == "LIKE" ) {
        if (field->getType() == FieldType::STRING && condition.type == FieldType::STRING)
            return likeMatch(field->getValue(), condition.value, condition.op);
        else
            return false;
    }
    return compare(field->getValue(), condition.value, condition.op);
}

std::vector<DocumentId> Query::binarySearchDocuments(
    const std::vector<std::pair<DocumentId, FieldValue>>& docs,
    const Condition& condition
) const {
    std::vector<DocumentId> result;

    auto compareWithQuery = [&](const std::pair<DocumentId, FieldValue>& lhs, const std::pair<DocumentId, FieldValue>& rhs) {
        //std::cout << "l: " << lhs.second << " r: " << rhs.second << "\n";
        return lhs.second < rhs.second;  // 正常比较
    };

    auto cond = std::make_pair(0, condition.value);

    // 执行比较和查找操作
    if (condition.op == "==") {
        auto range = std::equal_range(docs.begin(), docs.end(), cond, compareWithQuery);
        for (auto it = range.first; it != range.second; ++it) {
            result.push_back(it->first);  // 将符合条件的文档加入结果
        }
    } else if (condition.op == "<") {
        auto it = std::lower_bound(docs.begin(), docs.end(), cond, compareWithQuery);
        for (auto i = docs.begin(); i != it; ++i) {
            result.push_back(i->first);
        }
    } else if (condition.op == "<=") {
        auto it = std::upper_bound(docs.begin(), docs.end(), cond, compareWithQuery);
        for (auto i = docs.begin(); i != it; ++i) {
            result.push_back(i->first);
        }
    } else if (condition.op == ">") {
        auto it = std::upper_bound(docs.begin(), docs.end(), cond, compareWithQuery);
        for (auto i = it; i != docs.end(); ++i) {
            result.push_back(i->first);
        }
    } else if (condition.op == ">=") {
        auto it = std::lower_bound(docs.begin(), docs.end(), cond, compareWithQuery);
        for (auto i = it; i != docs.end(); ++i) {
            result.push_back(i->first);
        }
    } else if (condition.op == "!=") {
        result.reserve(docs.size());
        auto range = std::equal_range(docs.begin(), docs.end(), cond, compareWithQuery);
        for (auto& doc : docs) {
            if (doc < *range.first || doc >= *range.second) {
                result.push_back(doc.first);
            }
        }
    } else if (condition.op == "LIKE" && condition.type == FieldType::STRING) {
        // 对于 LIKE 的处理，这里假设字段是字符串类型
        for (const auto& doc : docs) {
            auto fieldValue = doc.second;  // 获取字段值
            if (getValueType(fieldValue) == FieldType::STRING && 
                likeMatch(fieldValue, condition.value, condition.op)) {
                result.push_back(doc.first);
            }
        }
    }

    return result;
}

void Query::match(std::vector<DocumentId>& candidateDocs) const {
    bool hasIdx = false;  // 有索引意味着正序
    bool sameIdx_sortPath = false; // 索引和排序重叠
    std::vector<size_t> indexedConditions;
    std::vector<size_t> nonIndexedConditions;

    // **1. 先遍历 conditions，分类索引和非索引条件**
    for (size_t i = 0; i < conditions.size(); ++i) {
        if (collection_.hasIndex(conditions[i].path)) {
            // **如果当前索引字段与排序字段相同，插入 indexedConditions 头部**
            if (!sorting.path.empty() && sorting.path == conditions[i].path) {
                indexedConditions.insert(indexedConditions.begin(), i);
                sameIdx_sortPath = true;
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
        hasIdx = true;

        // **首次使用索引，获取初始候选集**
        auto docs = collection_.getSortedDocuments(condition.path, candidateDocs);
        // **使用二分查找加速匹配**
        candidateDocs = binarySearchDocuments(docs, condition);

        if (candidateDocs.empty()) {
            return;  // 任何阶段筛选后为空，则提前返回
        }
    }

    // **3. 处理剩余的非索引 conditions**
    for (size_t idx : nonIndexedConditions) {
        const auto& condition = conditions[idx];
        std::vector<DocumentId> filteredDocs;

        if (candidateDocs.empty()) {
            // **如果之前没有索引查询，则从 documents_ 遍历所有文档**
            for (const auto& [docId, doc] : collection_.documents_) {
                if (matchCondition(doc, condition)) {
                    filteredDocs.emplace_back(docId);
                }
            }
        } else {
            // **基于已有候选集进一步筛选**
            for (const auto& docId : candidateDocs) {
                auto doc = collection_.getDocumentNoLock(docId);
                if (matchCondition(doc, condition)) {
                    filteredDocs.emplace_back(docId);
                }
            }
        }

        candidateDocs = std::move(filteredDocs);

        if (candidateDocs.empty()) {
            return;  // 任何阶段筛选后为空，则返回空
        }
    }

    // **4. 排序候选集**
    if (!hasIdx || !sameIdx_sortPath) {
        sort(candidateDocs);
    } else if (!sorting.ascending) {
        std::reverse(candidateDocs.begin(), candidateDocs.end());
    }
}

void Query::page(std::vector<DocumentId>& documents) {
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

void Query::sort(std::vector<DocumentId>& documents) const{
    if (sorting.path.empty()) return;

    bool useIndex = collection_.hasIndex(sorting.path);  // 是否有索引

    if (useIndex) {
        std::vector<DocumentId> sortedDocs;

        // 获取已排序的文档
        auto sortedDocuments = collection_.getSortedDocuments(sorting.path, documents);
        sortedDocs.reserve(documents.size());  // 预留合理的空间

        for (const auto& [docId, _ ] : sortedDocuments) {
            sortedDocs.emplace_back(docId);
        }

        // 逆序排序
        if (!sorting.ascending) {
            std::reverse(sortedDocs.begin(), sortedDocs.end());
        }
        documents.swap(sortedDocs);
    } else {
        std::vector<std::pair<DocumentId, std::pair<FieldValue, bool>>> cache;
        cache.reserve(documents.size());

        for (const auto& docID : documents) {
            auto doc = collection_.getDocumentNoLock(docID);
            if (!doc) continue;
            auto field = doc->getFieldByPath(sorting.path);
            FieldValue fieldValue;
            bool hasValue = false;
            if (field) {
                fieldValue = field->getValue();
                hasValue = true;
            }
            cache.emplace_back(docID, std::make_pair(fieldValue, hasValue));
        }
        /*sorting.ascending = true 时，缺失值的文档排在 后面。
        sorting.ascending = false 时，缺失值的文档排在 前面*/
        std::stable_sort(cache.begin(), cache.end(), [&](const auto& a, const auto& b) {
            if (!a.second.second) return !sorting.ascending;
            if (!b.second.second) return sorting.ascending;
            return sorting.ascending ? (a.second.first < b.second.first) : (a.second.first > b.second.first);
        });

        for (size_t i = 0; i < cache.size(); ++i) {
            documents[i] = cache[i].first;
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