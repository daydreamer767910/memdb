# core CMakeLists.txt

# 定义核心功能的库
add_library(dbcore STATIC field.cpp column.cpp table.cpp database.cpp)

# 如果有依赖其他模块，进行链接
target_link_libraries(dbcore PUBLIC util -lstdc++fs)

set_target_properties(dbcore PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)