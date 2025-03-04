# net CMakeLists.txt
FIND_PACKAGE( Boost 1.67 REQUIRED system )
# 查找 jemalloc
find_package(PkgConfig REQUIRED)
pkg_check_modules(JEMALLOC REQUIRED jemalloc)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
# 定义源文件
set(NET_SOURCES
    crypt.cpp
    transport.cpp
    transportmng.cpp
    tcpconnection.cpp
    transportclient.cpp
    transportsrv.cpp
)
# 定义网络功能的库
add_library(net STATIC ${NET_SOURCES})

# 如果有依赖其他模块，进行链接
target_link_libraries(net PUBLIC util log pthread ${Boost_LIBRARIES} ${JEMALLOC_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto argon2 ZLIB::ZLIB)

set_target_properties(net PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

