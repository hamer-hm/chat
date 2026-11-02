cmake_minimum_required(VERSION 3.16)

project(wechat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 让客户端输出到 bin 目录（可选）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Sql)

# ---------------------------------------------------------------
# 服务端：控制台程序
# ---------------------------------------------------------------
add_executable(server
    server/main.cpp
    server/database.h
    server/database.cpp
    server/server.h
    server/server.cpp
    common/protocol.h
)
target_include_directories(server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/server
)
target_link_libraries(server PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Sql
)

# ---------------------------------------------------------------
# 客户端：Windows GUI 程序（WIN32 表示不弹控制台窗口）
# ---------------------------------------------------------------
add_executable(client WIN32
    client/main.cpp
    client/networkclient.h
    client/networkclient.cpp
    client/loginwindow.h
    client/loginwindow.cpp
    client/messagebubble.h
    client/messagebubble.cpp
    client/mainwindow.h
    client/mainwindow.cpp
    common/protocol.h
)
target_include_directories(client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/client
)
target_link_libraries(client PRIVATE
    Qt6::Widgets
    Qt6::Network
)
