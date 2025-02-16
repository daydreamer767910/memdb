# client CMakeLists.txt
# 定义源文件
set(MDB_SOURCES
    client.cpp
    mdbclient.cpp
)

# lib for other apps
add_library(mdb SHARED ${MDB_SOURCES})
# 定义可执行文件 client
add_executable(client main.cpp)

# 链接需要的库（例如 core、net、util）
target_link_libraries(mdb PUBLIC net util pthread)
target_link_libraries(client PRIVATE crypt dbcore mdb)


set_target_properties(client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
set_target_properties(mdb PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)