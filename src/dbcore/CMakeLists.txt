# core CMakeLists.txt

# 定义核心功能的库
add_library(dbcore STATIC memcolumn.cpp memtable.cpp memdatabase.cpp)

# 如果有依赖其他模块，进行链接
target_link_libraries(dbcore PUBLIC util)
