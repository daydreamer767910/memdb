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

template <typename T>
class IObserver {
public:
    virtual void on_new_item(const std::shared_ptr<T>& item, uint32_t id) = 0;
    virtual void on_close_item(uint32_t id) = 0;
    virtual ~IObserver() = default;
};

// 定义数据类型的别名
using tcpMsg=std::tuple<char*, int, uint32_t>;
using appMsg=std::tuple<std::vector<uint8_t>*,int, uint32_t>;
// 定义可以在回调中处理的数据类型
using DataVariant = std::variant<tcpMsg, appMsg>;

// 定义接口类
class IDataCallback {
public:
    virtual void on_data_received(int len,int msg_id) = 0;  // 回调处理逻辑
    virtual DataVariant& get_data() = 0;  // 获取数据缓存
    virtual ~IDataCallback() = default; // 虚析构函数
};

#endif