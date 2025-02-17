# server CMakeLists.txt
file(GLOB_RECURSE SOURCE_FILES "handler/*.cpp")
# 定义可执行文件 server
add_executable(mdbsrv dbtask.cpp dbservice.cpp main.cpp ${SOURCE_FILES})

# 查找 jemalloc
find_package(PkgConfig REQUIRED)
pkg_check_modules(JEMALLOC REQUIRED jemalloc)
# 添加调试宏和编译选项
target_compile_definitions(mdbsrv PRIVATE UV_DEBUG)
target_compile_options(mdbsrv PRIVATE -g -O0 -Wall -Wextra)

# 链接需要的库（例如 core、net、util）
target_link_libraries(mdbsrv PRIVATE crypt dbcore net util log ${JEMALLOC_LIBRARIES})

set_target_properties(mdbsrv PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)