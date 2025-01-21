#ifndef COLLECTION_HPP
#define COLLECTION_HPP
#include <unordered_map>
#include <mutex>
#include "column.hpp"

class Table {
public:
    using ptr = std::shared_ptr<Table>;
    
    std::string name_;
    std::vector<Column> columns_;
    std::vector<Row> rows_;
    std::map<std::string, Index> indexes_;  // Indexes on the columns (if any)
    PrimaryKeyIndex primaryKeyIndex_; 

    Table(const std::string& tableName, const std::vector<Column>& columns);
    // Delete copy constructor and copy assignment operator
    Table(const Table&) = delete;
    Table& operator=(const Table&) = delete;

    // Default constructor (move semantics can be allowed)
    Table() = default;

    // Move constructor and move assignment operator (optional, if needed)
    Table(Table&&) = default;
    Table& operator=(Table&&) = default;

    // Methods related to rows and columns
    int insertRowsFromJson(const json& jsonRows);
    bool insertRow(const Row& row);
    bool insertRows(const std::vector<Row>& rows);
    json showRows();
    std::vector<Row> getRows() const;
    size_t getTotalRows() const;
        
    // Indexing methods
    void showIndexs();
    void addIndex(const std::string& columnName);
    std::vector<Row> queryByIndex(const std::string& columnName, const Field& value) const;
    std::vector<Row> getWithLimitAndOffset(int limit, int offset) const;
    //std::set<std::any> selectDistinct(const std::string& columnName) ;
    //std::map<std::any, int> groupBy(const std::string& columnName) ;

    json showTable();
    json rowsToJson(const std::vector<Row>& rows);
    json tableToJson();
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

#endif // Table_H
