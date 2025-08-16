cmake_minimum_required(VERSION 3.10)

project(client)

find_package(X11 REQUIRED)

include_directories(
    src/client
    src/client/screen_grabber
    third_party/stb
)

add_executable(client
    src/main.cc
    src/client/client.cc
    src/client/screen_grabber/screen_grabber.cc
)

target_include_directories(client PRIVATE ${X11_INCLUDE_DIR})
target_link_libraries(client PRIVATE common ${X11_LIBRARIES})
