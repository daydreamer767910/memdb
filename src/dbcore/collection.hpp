#ifndef COLLECTION_HPP
#define COLLECTION_HPP
#include <unordered_map>
#include <mutex>
#include "memcolumn.hpp"

class MemTable {
public:
    using ptr = std::shared_ptr<MemTable>;
    
    std::string name_;
    std::vector<Column> columns_;
    std::vector<Row> rows_;
    std::map<std::string, Index> indexes_;  // Indexes on the columns (if any)
    PrimaryKeyIndex primaryKeyIndex_; 

    MemTable(const std::string& tableName, const std::vector<Column>& columns);
    // Delete copy constructor and copy assignment operator
    MemTable(const MemTable&) = delete;
    MemTable& operator=(const MemTable&) = delete;

    // Default constructor (move semantics can be allowed)
    MemTable() = default;

    // Move constructor and move assignment operator (optional, if needed)
    MemTable(MemTable&&) = default;
    MemTable& operator=(MemTable&&) = default;

    // Methods related to rows and columns
    int insertRowsFromJson(const nlohmann::json& jsonRows);
    bool insertRow(const Row& row);
    bool insertRows(const std::vector<Row>& rows);
    nlohmann::json showRows();
    std::vector<Row> getRows() const;
    size_t getTotalRows() const;
        
    // Indexing methods
    void showIndexs();
    void addIndex(const std::string& columnName);
    std::vector<Row> queryByIndex(const std::string& columnName, const Field& value) const;
    std::vector<Row> getWithLimitAndOffset(int limit, int offset) const;
    //std::set<std::any> selectDistinct(const std::string& columnName) ;
    //std::map<std::any, int> groupBy(const std::string& columnName) ;

    nlohmann::json showTable();
    nlohmann::json rowsToJson(const std::vector<Row>& rows);
    nlohmann::json tableToJson();
    void exportToFile(const std::string& filePath) ;
    void importRowsFromFile(const std::string& filePath);
    
private:
    bool validateRow(const Row& row) ;
    bool validatePrimaryKey(const Row& row) ;
    void updateIndexes(const Row& row, int rowIndex);
    void updateIndexesBatch(const std::vector<size_t>& rowIdxes);
    Row processRowDefaults(const Row& row) const;

private:
    std::mutex mutex_;
};

#endif // MEMTABLE_H
