#include "threadbase.hpp"

// 定义输出运算符
std::ostream& operator<<(std::ostream& os, const task_status& status) {
    switch (status) {
        case task_status::INIT:
            os << "INIT";
            break;
        case task_status::RUNNING:
            os << "RUNNING";
            break;
        case task_status::IDLE:
            os << "IDLE";
            break;
        case task_status::TERMINITATE:
            os << "TERMINITATE";
            break;
    }
    return os;
}

