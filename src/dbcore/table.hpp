#ifndef Table_HPP
#define Table_HPP
#include "datacontainer.hpp"
#include "column.hpp"
#include "field.hpp"

using Row = std::vector<Field>;
// Define an index type
using Index = std::map<Field, std::set<size_t>>;

// 定义主键索引
using PrimaryKeyIndex = std::unordered_map<Field, size_t, Field::Hash>;

class Table: public DataContainer {
public:
    using ptr = std::shared_ptr<Table>;

    explicit Table(const std::string& name) : DataContainer(name) {}
    //Table(const std::string& tableName, const std::vector<Column>& columns);
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
    void buildIndex();
    std::vector<Row> getWithLimitAndOffset(int limit, int offset) const;
    
    size_t getColumnIndex(const std::string& columnName) const;
    std::vector<std::string> getColumnTypes(const std::vector<std::string>& columnNames) const;
    std::string getColumnType(const std::string& columnName) const;
    bool isPrimaryKey(const std::string& columnName) const;

    
    std::vector<std::vector<FieldValue>> query(
        const std::vector<std::string>& columnNames,
        const std::vector<std::string>& conditions,   // 查询条件列
        const std::vector<FieldValue>& queryValues,        // 查询条件值
        const std::vector<std::string>& operators,     // 比较操作符（对应每个条件）
        int limit = 100
    ) const;
    size_t update(
        const std::vector<std::string>& columnNames,  // 待更新的列名
        const std::vector<FieldValue>& newValues,          // 新值
        const std::vector<std::string>& conditions,   // 查询条件列
        const std::vector<FieldValue>& queryValues,        // 查询条件值
        const std::vector<std::string>& operators     // 比较操作符（对应每个条件）
    );
    size_t remove(
        const std::vector<std::string>& conditions,   // 查询条件列
        const std::vector<FieldValue>& queryValues,        // 查询条件值
        const std::vector<std::string>& operators     // 比较操作符（对应每个条件）
    );

    json rowsToJson(const std::vector<Row>& rows);
    std::vector<Row> jsonToRows(const json& jsonRows);
    Row jsonToRow(const json& jsonRow);
    virtual json toJson() const override;
    virtual void fromJson(const json& j) override;
    //void exportToFile(const std::string& filePath) ;
    //void importFromFile(const std::string& filePath);
    virtual void saveSchema(const std::string& filePath) override;
    virtual void exportToBinaryFile(const std::string& filePath) override;
    virtual void importFromBinaryFile(const std::string& filePath) override;
private:
    bool isValidType(const FieldValue& value, const std::string& type) const;
    bool validateRow(const Row& row) ;
    bool validatePrimaryKey(const Row& row) ;
    void updateIndexes(const Row& row, int rowIndex);
    void updateIndexesBatch(const std::vector<size_t>& rowIdxes);
    Row processRowDefaults(const Row& row) const;

    std::vector<size_t> matchPrimaryKey(
        const std::vector<size_t>& rowSet,
        const FieldValue& queryValue,
        const std::string& op,
        const std::string& columnName
    ) const;
    std::vector<size_t> matchIndex(
        const std::vector<size_t>& rowSet,
        const FieldValue& queryValue,
        const std::string& op,
        const std::string& columnName
    ) const;
    std::vector<size_t> search(const std::vector<std::string>& conditions,   // 查询条件列
        const std::vector<FieldValue>& queryValues,        // 查询条件值
        const std::vector<std::string>& operators     // 比较操作符（对应每个条件）
    ) const;
private:
    std::vector<Column> columns_;
    std::vector<Row> rows_;
    std::map<std::string, Index> indexes_;  // Indexes on the columns (if any)
    PrimaryKeyIndex primaryKeyIndex_; 
};

#endif // Table_H
