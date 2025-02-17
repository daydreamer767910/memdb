# keymng CMakeLists.txt
# 查找 libsodium 库
find_package(PkgConfig REQUIRED)
pkg_search_module(SODIUM REQUIRED libsodium)
pkg_check_modules(JEMALLOC REQUIRED jemalloc)
# 设置包含目录
include_directories(${SODIUM_INCLUDE_DIRS})

# 设置库链接目录
link_directories(${SODIUM_LIBRARY_DIRS})

# lib for project
add_library(crypt STATIC crypt.cpp)
# 生成可执行文件 keymng
add_executable(keymng crypt.cpp main.cpp)

# 链接需要的 libsodium 库
target_link_libraries(crypt PUBLIC ${SODIUM_LIBRARIES} ${JEMALLOC_LIBRARIES})
target_link_libraries(keymng PRIVATE dbcore ${SODIUM_LIBRARIES} ${JEMALLOC_LIBRARIES})

# 设置目标的输出目录
set_target_properties(keymng PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

set_target_properties(crypt PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
