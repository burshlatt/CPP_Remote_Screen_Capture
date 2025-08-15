cmake_minimum_required(VERSION 3.10)

project(client)

find_package(X11 REQUIRED)

include_directories(
    src/client
    src/client/unique_fd
    src/client/screen_grabber
    src/client/screen_grabber/unique_display
    src/client/screen_grabber/unique_x_image
    third_party/stb
)

add_executable(client
    src/main.cc
    src/client/client.cc
    src/client/unique_fd/unique_fd.cc
    src/client/screen_grabber/screen_grabber.cc
    src/client/screen_grabber/unique_display/unique_display.cc
    src/client/screen_grabber/unique_x_image/unique_x_image.cc
)

target_include_directories(client PRIVATE ${X11_INCLUDE_DIR})
target_link_libraries(client PRIVATE ${X11_LIBRARIES})
