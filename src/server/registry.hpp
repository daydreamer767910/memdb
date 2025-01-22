#ifndef ACTION_REGISTRY_HPP
#define ACTION_REGISTRY_HPP

#include <string>
#include <unordered_map>
#include <memory>
#include "action.hpp"

class ActionRegistry {
public:
    using HandlerFactory = std::function<std::unique_ptr<ActionHandler>()>;

    // 注册处理器工厂
    void registerAction(const std::string& actionName, HandlerFactory factory) {
        if (registry_.count(actionName)) {
            throw std::runtime_error("Action already registered: " + actionName);
        }
        registry_[actionName] = std::move(factory);
    }

    // 获取处理器实例
    std::unique_ptr<ActionHandler> getHandler(const std::string& actionName) {
        auto it = registry_.find(actionName);
        if (it != registry_.end()) {
            return it->second();
        }
        throw std::runtime_error("No handler registered for action: " + actionName);
    }

    // 全局访问点
    static ActionRegistry& getInstance() {
        static ActionRegistry instance;
        return instance;
    }

private:
    std::unordered_map<std::string, HandlerFactory> registry_;
};

#define REGISTER_ACTION(actionName, handlerType)                          \
    namespace {                                                           \
        const bool handlerType##_registered = []() {                      \
            ActionRegistry::getInstance().registerAction(actionName, []() { \
                return std::make_unique<handlerType>();                   \
            });                                                           \
            return true;                                                  \
        }();                                                              \
    }

#endif // ACTION_REGISTRY_HPP
