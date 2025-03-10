# 设置 CMake 最低版本要求
cmake_minimum_required(VERSION 3.12)

# 设置项目名称
project(mdb-project VERSION 1.0.1 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
# 为所有目标添加 -fPIC 选项
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 设置调试模式下的编译选项
if(DEFINED ENV{NODE_ENV} AND "$ENV{NODE_ENV}" STREQUAL "production")
    message(STATUS "NODE_ENV is production: Using default flags")
    set(CMAKE_CXX_FLAGS_DEBUG "-O3 -DNDEBUG -Wall -Wextra") 
else()
    message(STATUS "NODE_ENV is not production: Enabling debug flags")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
    # 为整个项目添加调试宏
    add_definitions(-DDEBUG)
endif()

# 添加 src 目录到包含路径
include_directories(${CMAKE_SOURCE_DIR}/src)

# 添加各个子模块
add_subdirectory(src/keymng ${CMAKE_BINARY_DIR}/keymng)
add_subdirectory(src/client ${CMAKE_BINARY_DIR}/client)
add_subdirectory(src/server ${CMAKE_BINARY_DIR}/mdbsrv)
add_subdirectory(src/dbcore ${CMAKE_BINARY_DIR}/dbcore)
add_subdirectory(src/log ${CMAKE_BINARY_DIR}/log)
add_subdirectory(src/net ${CMAKE_BINARY_DIR}/net)
add_subdirectory(src/util ${CMAKE_BINARY_DIR}/util)

# 安装目标
install(TARGETS keymng client mdbsrv net util mdb dbcore log
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# 安装头文件
#install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/include DESTINATION include)
