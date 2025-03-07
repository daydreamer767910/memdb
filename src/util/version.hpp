#ifndef VERSION_HPP
#define VERSION_HPP

#include <iostream>

#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)
// 定义版本字符串宏
#define PROJECT_VERSION "version " TO_STRING(PROJECT_VERSION_MAJOR) "." \
                        TO_STRING(PROJECT_VERSION_MINOR) "." \
                        TO_STRING(PROJECT_VERSION_PATCH)

// get_version() 直接返回 `PROJECT_VERSION`
#define GET_VERSION() std::string(PROJECT_VERSION)

#endif