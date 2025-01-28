#include "Query.hpp"

Query& Query::condition(const std::string& path, const FieldValue& value, const std::string& op) {
	conditions.push_back({op, path, value});
    return *this;
}
// 排序方法
Query& Query::orderBy(const std::string& path, bool ascending) {
    sorting = {path, ascending};
    return *this;
}

Query& Query::project(const std::vector<std::string>& fields) {
    this->fieldsToProject = fields;
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
std::vector<std::shared_ptr<Document>> Query::execute(const std::vector<std::shared_ptr<Document>>& documents) const {
    std::vector<std::shared_ptr<Document>> results;
    // 条件匹配
    for (const auto& doc : documents) {
        bool match = true;
        for (const auto& condition : conditions) {
            // 获取字段值
            auto field = doc->getFieldByPath(condition.path);
            if (!field) {
                match = false;
                break;
            }
            const FieldValue& fieldValue = field->getValue();

            // 使用 std::visit 判断值是否匹配
            match = std::visit([&](const auto& value) -> bool {
                if (condition.op == "regex") {
                    if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::string>) {
                        std::regex regex(std::get<std::string>(condition.value));
                        return std::regex_match(value, regex);
                    }
                    return false;  // 非字符串类型无法匹配正则
                } else {
                    return compare(value, condition.value, condition.op);
                }
            }, fieldValue);

            if (!match) break;
        }

        // 如果匹配，处理投影字段
        if (match) {
            if (fieldsToProject.empty()) {
                results.push_back(doc);  // 返回完整文档
            } else {
                auto projectedDoc = std::make_shared<Document>();
                for (const auto& field : fieldsToProject) {
                    auto it = doc->getFieldByPath(field);
                    projectedDoc->setField(field,it);  // 投影指定字段
                }
                results.push_back(projectedDoc);
            }
        }
    }
    // 排序
    if (!sorting.path.empty()) {
        std::sort(results.begin(), results.end(), [&](const std::shared_ptr<Document>& a, const std::shared_ptr<Document>& b) {
            const auto& va = a->getFieldByPath(sorting.path)->getValue();
            const auto& vb = b->getFieldByPath(sorting.path)->getValue();
            return sorting.ascending ? va < vb : va > vb;
        });
    }

    // 分页
    if (startIndex < results.size()) {
        results = std::vector<std::shared_ptr<Document>>(results.begin() + startIndex, results.end());
    }
    if (maxResults > 0 && results.size() > maxResults) {
        results.resize(maxResults);
    }

    return results;
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