cmake_minimum_required(VERSION 3.10)

project(server)

include_directories(
    src/server
    src/server/session
    src/server/unique_fd
)

add_executable(server
    src/main.cc
    src/server/server.cc
    src/server/session/session.cc
    src/server/unique_fd/unique_fd.cc
)

