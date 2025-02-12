#ifndef QUERY_HPP
#define QUERY_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <regex>
#include <optional>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <ctime>
#include <memory>
#include "fieldvalue.hpp"
#include "document.hpp"
#include "collection.hpp"

class Query {
private:
    struct Condition {
        std::string op;
        std::string path;
        FieldValue value;
        FieldType type;
    };

    struct Sorting {
        std::string path;
        bool ascending;
    };

    std::vector<Condition> conditions;
    Sorting sorting;
    size_t maxResults = 0;
    size_t startIndex = 0;
private:
    const Collection& collection_;
public:
    Query(const Collection& collection) : collection_(collection) {}
    Query& condition(const std::string& path, const FieldValue& value, const std::string& op);
    Query& orderBy(const std::string& path, bool ascending = true);
    Query& limit(size_t maxResults);
    Query& offset(size_t startIndex);
    bool matchCondition(const std::shared_ptr<Document>& doc, const Condition& condition) const;
    std::vector<DocumentId> binarySearchDocuments(
        const std::vector<std::pair<DocumentId, FieldValue>>& docs,
        const Condition& condition
    ) const;
    void match(std::vector<DocumentId>& candidateDocs) const;
	void sort(std::vector<DocumentId>& documents) const;
	void page(std::vector<DocumentId>& documents);
	// 从 JSON 创建 Query 对象
    Query& fromJson(const json& j);

};

#endif // QUERY_HPP
