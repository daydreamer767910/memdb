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

std::vector<std::pair<DocumentId, std::shared_ptr<Document>>> Query::binarySearchDocuments(
    const std::vector<std::pair<DocumentId, std::shared_ptr<Document>>>& docs,
    const Condition& condition
) const {
    std::vector<std::pair<DocumentId, std::shared_ptr<Document>>> result;

    auto compareWithQuery = [&](const std::pair<DocumentId, std::shared_ptr<Document>>& lhs, const std::pair<DocumentId, std::shared_ptr<Document>>& rhs) {
        auto lv = lhs.second->getFieldByPath(condition.path)->getValue();
        auto rv = rhs.second->getFieldByPath(condition.path)->getValue();
//std::cout << condition.path << " :lhs: " << lv << " rhs: " << rv << " \n";
        // 确保类型匹配，如果不匹配则返回 false
        if (lv.index() != rv.index()) {
            return false;  // 类型不匹配时返回 false
        }
        return lv < rv;
    };
    auto doc = std::make_shared<Document>();
    doc->setFieldByPath(condition.path, condition.value);
    auto cond = std::make_pair(std::string(""),doc);
    // 执行比较和查找操作
    if (condition.op == "==") {
        auto range = std::equal_range(docs.begin(), docs.end(), cond, compareWithQuery);
        for (auto it = range.first; it != range.second; ++it) {
            std::cout << condition.value << " == " << it->second->getFieldByPath(condition.path)->getValue() << " \n";
            result.push_back(*it);  // 将符合条件的文档加入结果
        }
    } else if (condition.op == "<") {
        auto it = std::lower_bound(docs.begin(), docs.end(), cond, compareWithQuery);
        for (auto i = docs.begin(); i != it; ++i) {
            result.push_back(*i);
        }
    } else if (condition.op == "<=") {
        auto it = std::upper_bound(docs.begin(), docs.end(), cond, compareWithQuery);
        for (auto i = docs.begin(); i != it; ++i) {
            result.push_back(*i);
        }
    } else if (condition.op == ">") {
        auto it = std::upper_bound(docs.begin(), docs.end(), cond, compareWithQuery);
        for (auto i = it; i != docs.end(); ++i) {
            result.push_back(*i);
        }
    } else if (condition.op == ">=") {
        auto it = std::lower_bound(docs.begin(), docs.end(), cond, compareWithQuery);
        for (auto i = it; i != docs.end(); ++i) {
            result.push_back(*i);
        }
    }

    return result;
}

void Query::match(std::vector<std::pair<DocumentId, std::shared_ptr<Document>>>& candidateDocs) const {
    bool hasIdx = false;//有索引意味着正序
    bool sameIdx_sortPath = false; //索引和排序重叠

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
        std::vector<std::pair<DocumentId, std::shared_ptr<Document>>> filteredDocs;
        hasIdx = true;
        if (i == 0) {
            // **首次使用索引，获取初始候选集**
            //std::cout << "use " << condition.path << " as 1st condition\n";
            auto docs = collection_.getSortedDocuments(condition.path);
            // **使用二分查找加速匹配**
            filteredDocs = binarySearchDocuments(docs, condition);
        } else {
            // **后续条件基于已有候选集筛选**
            filteredDocs = binarySearchDocuments(candidateDocs, condition);
        }

        candidateDocs = std::move(filteredDocs);

        if (candidateDocs.empty()) {
            return;  // 任何阶段筛选后为空，则提前返回
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
            return;  // 任何阶段筛选后为空，则返回空
        }
    }

    if (!hasIdx || !sameIdx_sortPath) {
        sort(candidateDocs);
    } else if (!sorting.ascending) {
        std::reverse(candidateDocs.begin(), candidateDocs.end());
    }
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

void Query::sort(std::vector<std::pair<DocumentId, std::shared_ptr<Document>>>& documents) const{
    if (sorting.path.empty()) return;

    bool useIndex = collection_.hasIndex(sorting.path);  // 是否有索引

    if (useIndex) {
        std::unordered_set<DocumentId> docIds;
        for (const auto& [id, doc] : documents) {
            docIds.insert(id);
        }

        auto sortedDocuments = collection_.getSortedDocuments(sorting.path);
        std::vector<std::pair<DocumentId, std::shared_ptr<Document>>> sortedDocs;
        sortedDocs.reserve(documents.size());

        for (const auto& [id, doc] : sortedDocuments) {
            if (docIds.find(id) != docIds.end()) {
                sortedDocs.emplace_back(id, doc);
            }
        }

        if (!sorting.ascending) {
            std::reverse(sortedDocs.begin(), sortedDocs.end());
        }
        documents.swap(sortedDocs);
    } else {
        std::vector<std::pair<std::pair<DocumentId, std::shared_ptr<Document>>, std::pair<FieldValue, bool>>> cache;
        cache.reserve(documents.size());

        for (const auto& docPair : documents) {
            auto field = docPair.second->getFieldByPath(sorting.path);
            FieldValue fieldValue;
            bool hasValue = false;
            if (field) {
                fieldValue = field->getValue();
                hasValue = true;
            }
            cache.emplace_back(docPair, std::make_pair(fieldValue, hasValue));
        }

        std::stable_sort(cache.begin(), cache.end(), [&](const auto& a, const auto& b) {
            if (!a.second.second) return sorting.ascending;
            if (!b.second.second) return !sorting.ascending;
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