#ifndef MEMTABLE_HPP
#define MEMTABLE_HPP
#include <unordered_map>
#include <shared_mutex>
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
    std::optional<Row> findRowByPrimaryKey(const Field& primaryKey) const;
    size_t getColumnIndex(const std::string& columnName) const;
    std::vector<std::string> getColumnTypes(const std::vector<std::string>& columnNames) const;
    std::string getColumnType(const std::string& columnName) const;
    std::vector<Row> query(const std::string& columnName,const std::function<bool(const Field&)>& predicate) const;
    std::vector<Row> query(const std::string& columnName,const std::string& op,const Field& queryValue) const;
    bool update(
        const std::vector<std::string>& columnNames,  // 待更新的列名
        const std::vector<Field>& newValues,          // 新值
        const std::vector<std::string>& conditions,   // 查询条件列
        const std::vector<Field>& queryValues,        // 查询条件值
        const std::vector<std::string>& operators     // 比较操作符（对应每个条件）
    );
    json showTable();
    json rowsToJson(const std::vector<Row>& rows);
    std::vector<Row> jsonToRows(const json& jsonRows);
    Row jsonToRow(const json& jsonRow);
    json tableToJson();
    void exportToFile(const std::string& filePath) ;
    void importFromFile(const std::string& filePath);
    void exportToBinaryFile(const std::string& filePath);
    void importFromBinaryFile(const std::string& filePath);
private:
    bool validateRow(const Row& row) ;
    bool validatePrimaryKey(const Row& row) ;
    void updateIndexes(const Row& row, int rowIndex);
    void updateIndexesBatch(const std::vector<size_t>& rowIdxes);
    Row processRowDefaults(const Row& row) const;

private:
    mutable std::shared_mutex mutex_;
};

#endif // MEMTABLE_H
