# keymng CMakeLists.txt

find_package(PkgConfig REQUIRED)
pkg_check_modules(JEMALLOC REQUIRED jemalloc)

# 生成可执行文件 keymng
add_executable(keymng main.cpp)

# 链接需要的
target_link_libraries(keymng PRIVATE net dbcore ${JEMALLOC_LIBRARIES})

# 设置目标的输出目录
set_target_properties(keymng PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

