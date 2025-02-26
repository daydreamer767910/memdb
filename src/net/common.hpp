#ifndef COMMON_HPP
#define COMMON_HPP
#include <variant>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <type_traits>
#include <stdexcept>
#include <condition_variable>
#include "util/util.hpp"

class Transport;
class ITransportObserver {
public:
    virtual void on_new_transport(const std::shared_ptr<Transport>& transport) = 0;
	virtual void on_close_transport(const uint32_t port_id) = 0;
    virtual ~ITransportObserver() = default;
};

class IConnection;
class IConnectionObserver {
public:
    virtual void on_new_connection(const std::shared_ptr<IConnection>& connection) = 0;
	virtual void on_close_connection(const uint32_t port_id) = 0;
    virtual ~IConnectionObserver() = default;
};


// 定义数据类型的别名
using tcpMsg=std::tuple<char*, int, uint32_t>;
using appMsg=std::tuple<std::vector<uint8_t>*,int, uint32_t>;
// 定义可以在回调中处理的数据类型
using DataVariant = std::variant<tcpMsg, appMsg>;

// 定义接口类
class IDataCallback :public std::enable_shared_from_this<IDataCallback>{
public:
    virtual void on_data_received(int result,int id) = 0;  // 回调处理逻辑
    virtual DataVariant& get_data() = 0;  // 获取数据缓存
    virtual ~IDataCallback() = default; // 虚析构函数
};

class IConnection : public IDataCallback {
public:
    virtual void set_transport(const std::shared_ptr<Transport>& transport) = 0;
    virtual ~IConnection() = default;
};

#endif