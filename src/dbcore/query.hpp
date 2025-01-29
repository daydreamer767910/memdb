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


class Query {
public:
    Query& condition(const std::string& path, const FieldValue& value, const std::string& op);
    Query& orderBy(const std::string& path, bool ascending = true);
    Query& limit(size_t maxResults);
    Query& offset(size_t startIndex);
    bool match(const std::shared_ptr<Document>& document);
	void sort(std::vector<std::shared_ptr<Document>>& documents);
	std::vector<std::shared_ptr<Document>> page(const std::vector<std::shared_ptr<Document>>& documents);
	// 从 JSON 创建 Query 对象
    Query& fromJson(const json& j);
private:
    struct Condition {
        std::string op;
        std::string path;
        FieldValue value;
    };

    struct Sorting {
        std::string path;
        bool ascending;
    };

    std::vector<Condition> conditions;
    Sorting sorting;
    size_t maxResults = 0;
    size_t startIndex = 0;
};

#endif // QUERY_HPP
