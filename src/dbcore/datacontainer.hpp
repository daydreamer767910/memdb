#ifndef DATACONTAINER_HPP
#define DATACONTAINER_HPP

#include <memory>
#include <mutex>
#include <unordered_map>
#include <shared_mutex>
#include "util/util.hpp"

class DataContainer {
public:
	using ptr = std::shared_ptr<DataContainer>;
    virtual ~DataContainer() = default;

    // 通用接口
    std::string getName() const{
		return name_;
	};
	std::string getType() const{
		return type_;
	};
    virtual json toJson() const = 0;
    virtual void fromJson(const json& j) = 0;

    // 其他可以抽象的功能
    //virtual size_t getRowCount() const = 0;
	virtual void saveSchema(const std::string& filePath)= 0;
	virtual void exportToBinaryFile(const std::string& filePath) = 0;
    virtual void importFromBinaryFile(const std::string& filePath) = 0;
protected:
	explicit DataContainer(const std::string& name, const std::string& type) : name_(name),type_(type) {}
    std::string name_;
	std::string type_;
    mutable std::shared_mutex mutex_;
};

#endif