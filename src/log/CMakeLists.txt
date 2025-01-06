# log CMakeLists.txt

# 定义静态库 lib
add_library(log STATIC logger.cpp)

# 设置库的依赖（例如 core、net）
target_link_libraries(log PUBLIC util)
