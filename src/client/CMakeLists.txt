# client CMakeLists.txt
# 定义源文件
set(MDB_SOURCES
    client.cpp
)

# lib for other apps
add_library(mdb SHARED ${MDB_SOURCES})
# 定义可执行文件 client
add_executable(client main.cpp)
# 查找 jemalloc
find_package(PkgConfig REQUIRED)
pkg_check_modules(JEMALLOC REQUIRED jemalloc)
# 添加调试宏和编译选项
target_compile_definitions(mdb PRIVATE 
    PROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    PROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    PROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH})
target_compile_definitions(client PRIVATE 
    PROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    PROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    PROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH})
# 链接需要的库（例如 core、net、util）
target_link_libraries(mdb PUBLIC net util pthread ${JEMALLOC_LIBRARIES})
target_link_libraries(client PRIVATE crypt dbcore mdb ${JEMALLOC_LIBRARIES})


set_target_properties(client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
set_target_properties(mdb PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)