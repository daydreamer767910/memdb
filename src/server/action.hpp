#ifndef ACTIONHANDLER_HPP
#define ACTIONHANDLER_HPP

#include "dbcore/database.hpp"
#include "util/util.hpp"
// 动作处理器基类
class ActionHandler {
public:
    virtual void handle(const json& task, Database::ptr db, json& response) = 0;
    virtual ~ActionHandler() = default;
};

#endif