# client CMakeLists.txt
# 查找 Boost 库
FIND_PACKAGE( Boost 1.67 REQUIRED system )
# 定义可执行文件 client
add_executable(client main.cpp mdbclient.cpp)

# 链接需要的库（例如 core、net、util）
target_link_libraries(client PRIVATE net util ${Boost_LIBRARIES})

set_target_properties(client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)