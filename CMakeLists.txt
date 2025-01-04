cmake_minimum_required(VERSION 3.10)
project(mdb-project VERSION 1.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 查找依赖库
find_package(nlohmann_json REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(UV REQUIRED libuv)

# 查找所有源文件
file(GLOB_RECURSE SOURCES "src/*.cpp")
# 查找所有头文件
file(GLOB_RECURSE HEADERS "src/*.hpp")

# 添加库文件
add_library(mdb SHARED ${SOURCES})
# 添加可执行文件
add_executable(mdbsrv ${SOURCES})

# 设置包含路径（直接包含 src 根目录）
target_include_directories(mdb PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(mdbsrv PRIVATE ${CMAKE_SOURCE_DIR}/src)

# 链接库
target_link_libraries(mdb PRIVATE nlohmann_json::nlohmann_json ${UV_LIBRARIES})
target_link_libraries(mdbsrv PRIVATE mdb ${UV_LIBRARIES})

# 指定输出目录
set_target_properties(mdb PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
set_target_properties(mdbsrv PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 安装目标
install(TARGETS mdb mdbsrv
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# 安装头文件
#install(FILES ${HEADERS} DESTINATION include)

