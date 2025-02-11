# net CMakeLists.txt
FIND_PACKAGE( Boost 1.67 REQUIRED system )
# 定义源文件
set(NET_SOURCES
    transport.cpp
    transportsrv.cpp
    tcpconnection.cpp
    tcpserver.cpp
)
# 定义网络功能的库
add_library(net STATIC ${NET_SOURCES})

# 如果有依赖其他模块，进行链接
target_link_libraries(net PUBLIC crypt util log pthread ${Boost_LIBRARIES})

set_target_properties(net PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

