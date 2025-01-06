# server CMakeLists.txt

# 定义可执行文件 server
add_executable(mdbsrv dbservice.cpp main.cpp)


# 添加调试宏和编译选项
target_compile_definitions(mdbsrv PRIVATE UV_DEBUG)
target_compile_options(mdbsrv PRIVATE -g -O0 -Wall -Wextra)

# 链接需要的库（例如 core、net、util）
target_link_libraries(mdbsrv PRIVATE dbcore net util log)

set_target_properties(mdbsrv PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)