# keymng CMakeLists.txt
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JEMALLOC REQUIRED jemalloc)

# lib for project
add_library(crypt STATIC crypt.cpp)
# 生成可执行文件 keymng
add_executable(keymng crypt.cpp main.cpp)

# 链接需要的 libsodium 库
target_link_libraries(crypt PUBLIC ${JEMALLOC_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto argon2)
target_link_libraries(keymng PRIVATE dbcore ${JEMALLOC_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto argon2)

# 设置目标的输出目录
set_target_properties(keymng PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

set_target_properties(crypt PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
