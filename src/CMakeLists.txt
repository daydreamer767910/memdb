cmake_minimum_required(VERSION 3.10)
project(mdb-project VERSION 1.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 查找依赖库
find_package(nlohmann_json REQUIRED) # nlohmann/json 库支持
#find_package(uv REQUIRED) # 查找 libuv
find_package(PkgConfig REQUIRED)
pkg_check_modules(UV REQUIRED libuv)

# 源文件
set(SOURCE_FILES util.cpp threadbase.cpp logger.cpp memcolumn.cpp memtable.cpp memdatabase.cpp tcpserver.cpp main.cpp )

# 添加库文件
add_library(mdb SHARED ${SOURCE_FILES})
# 添加可执行文件
add_executable(mdbsrv ${SOURCE_FILES})

# 包含目录
#target_include_directories(mdb PRIVATE ${nlohmann_json_INCLUDE_DIRS} ${UV_INCLUDE_DIR})
#include_directories(${nlohmann_json_INCLUDE_DIRS})
target_link_libraries(mdb PRIVATE nlohmann_json::nlohmann_json ${UV_LIBRARIES})
target_link_libraries(mdbsrv PRIVATE ${UV_LIBRARIES})


# 指定输出目录
set_target_properties(mdb PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ../lib
    LIBRARY_OUTPUT_DIRECTORY ../lib
)
set_target_properties(mdbsrv PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ../bin
)

# Install library
install(TARGETS mdbsrv mdb
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# 安装头文件（如果需要）
install(FILES memtable.hpp memdatabase.hpp DESTINATION include)

