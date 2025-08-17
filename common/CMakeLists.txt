add_library(common STATIC
    src/logger.cc
    src/input_parser.cc
    src/resource_factory.cc
)

target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
