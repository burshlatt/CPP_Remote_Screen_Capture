cmake_minimum_required(VERSION 3.10)

project(server)

find_package(X11 REQUIRED)

include_directories(
    src/server
    src/server/session
)

add_executable(server
    src/main.cc
    src/server/server.cc
    src/server/session/session.cc
)

target_include_directories(server PRIVATE ${X11_INCLUDE_DIR})
target_link_libraries(server PRIVATE common ${X11_LIBRARIES})
