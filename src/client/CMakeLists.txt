# client CMakeLists.txt

# 定义可执行文件 client
add_executable(client main.cpp)

# 链接需要的库（例如 core、net、util）
target_link_libraries(client PRIVATE net util)

set_target_properties(client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)