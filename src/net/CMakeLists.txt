# net CMakeLists.txt
# 定义源文件
set(NET_SOURCES
    transport.cpp
    transportsrv.cpp
    tcpconnection.cpp
    tcpserver.cpp
)
# 定义网络功能的库
add_library(net SHARED ${NET_SOURCES})

# 如果有依赖其他模块，进行链接
target_link_libraries(net PUBLIC util log)

set_target_properties(net PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

