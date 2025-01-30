#include "query.hpp"

Query& Query::condition(const std::string& path, const FieldValue& value, const std::string& op) {
	conditions.push_back({op, path, value});
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

// 执行查询
bool Query::match(const std::shared_ptr<Document>& document) {
    // 条件匹配
	bool match = true;
	for (const auto& condition : conditions) {
		// 获取字段值
		auto field = document->getFieldByPath(condition.path);
		if (!field) {
			match = false;
			break;
		}
		const FieldValue& fieldValue = field->getValue();
        const FieldType& fieldType = field->getType();
        if (Field(condition.value).getType() != fieldType) {
            match = false;
			break;
        }
		// 使用 std::visit 判断值是否匹配
		match = std::visit([&](const auto& value) -> bool {
			/*if (condition.op == "regex") {//效率太低 不建议支持regex匹配
				if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::string>) {
					std::regex regex(std::get<std::string>(condition.value));
					return std::regex_match(value, regex);
				}
				return false;  // 非字符串类型无法匹配正则
			} else {*/
				return compare(value, condition.value, condition.op);
			//}
		}, fieldValue);

		if (!match) break;
	}
    return match;
}

std::vector<std::shared_ptr<Document>> Query::page(const std::vector<std::shared_ptr<Document>>& documents) {
	// 分页
	std::vector<std::shared_ptr<Document>> results;
    if (startIndex < documents.size()) {
        results = std::vector<std::shared_ptr<Document>>(documents.begin() + startIndex, documents.end());
    }
    if (maxResults > 0 && results.size() > maxResults) {
        results.resize(maxResults);
    }
    return results;
}

void Query::sort(std::vector<std::shared_ptr<Document>>& documents) {
    if (!sorting.path.empty()) {
        // 缓存目标字段值，避免重复调用 getFieldByPath
        std::vector<std::pair<std::shared_ptr<Document>, std::optional<FieldValue>>> cache;
        for (const auto& doc : documents) {
            auto field = doc->getFieldByPath(sorting.path);
            if (field) {
                cache.emplace_back(doc, field->getValue());
            } else {
                cache.emplace_back(doc, std::nullopt); // 无法找到字段
            }
        }

        // 使用 std::stable_sort 保证排序稳定性
        std::stable_sort(cache.begin(), cache.end(), [&](const auto& a, const auto& b) {
            const auto& va = a.second;
            const auto& vb = b.second;

            // 如果某个字段不存在，优先排后面
            if (!va) return sorting.ascending;
            if (!vb) return !sorting.ascending;

            // 比较值
            return sorting.ascending ? (*va < *vb) : (*va > *vb);
        });

        // 重新排列 documents
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