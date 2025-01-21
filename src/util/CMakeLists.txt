# util CMakeLists.txt
# 查找依赖库
find_package(nlohmann_json REQUIRED)

# 定义工具函数的库
add_library(util STATIC util.cpp)

# 如果有依赖其他模块，进行链接
target_link_libraries(util PUBLIC nlohmann_json::nlohmann_json)

set_target_properties(util PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)