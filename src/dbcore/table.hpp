#ifndef Table_HPP
#define Table_HPP
#include <unordered_map>
#include <shared_mutex>
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
    void buildIndex();
    std::vector<Row> queryByIndex(const std::string& columnName, const Field& value) const;
    std::vector<Row> getWithLimitAndOffset(int limit, int offset) const;
    std::optional<Row> findRowByPrimaryKey(const Field& primaryKey) const;
    
    size_t getColumnIndex(const std::string& columnName) const;
    std::vector<std::string> getColumnTypes(const std::vector<std::string>& columnNames) const;
    std::string getColumnType(const std::string& columnName) const;
    bool isPrimaryKey(const std::string& columnName) const;

    
    std::vector<std::vector<Field>> query(
        const std::vector<std::string>& columnNames,
        const std::vector<std::string>& conditions,   // 查询条件列
        const std::vector<Field>& queryValues,        // 查询条件值
        const std::vector<std::string>& operators,     // 比较操作符（对应每个条件）
        int limit = 100
    ) const;
    size_t update(
        const std::vector<std::string>& columnNames,  // 待更新的列名
        const std::vector<Field>& newValues,          // 新值
        const std::vector<std::string>& conditions,   // 查询条件列
        const std::vector<Field>& queryValues,        // 查询条件值
        const std::vector<std::string>& operators     // 比较操作符（对应每个条件）
    );
    size_t remove(
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

    std::vector<size_t> matchPrimaryKey(
        const std::vector<size_t>& rowSet,
        const Field& queryValue,
        const std::string& op,
        const std::string& columnName
    ) const;
    std::vector<size_t> matchIndex(
        const std::vector<size_t>& rowSet,
        const Field& queryValue,
        const std::string& op,
        const std::string& columnName
    ) const;
    std::vector<size_t> search(const std::vector<std::string>& conditions,   // 查询条件列
        const std::vector<Field>& queryValues,        // 查询条件值
        const std::vector<std::string>& operators     // 比较操作符（对应每个条件）
    ) const;
private:
    mutable std::shared_mutex mutex_;
};

#endif // Table_H
