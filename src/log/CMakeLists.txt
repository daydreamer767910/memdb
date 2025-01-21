# log CMakeLists.txt

# 定义静态库 lib
add_library(log STATIC logger.cpp)

# 设置库的依赖（例如 core、net）
target_link_libraries(log PUBLIC util)

set_target_properties(log PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)