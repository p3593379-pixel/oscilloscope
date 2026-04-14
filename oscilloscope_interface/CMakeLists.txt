cmake_minimum_required(VERSION 3.25)
project(oscilloscope_interface VERSION 0.1.0 LANGUAGES CXX)

find_package(Protobuf REQUIRED)

add_library(oscilloscope_interface_proto STATIC)

protobuf_generate(
    TARGET    oscilloscope_interface_proto
    LANGUAGE  cpp
    PROTOS    oscilloscope_interface.proto
    OUT_VAR   PROTO_GENERATED_SRCS
)

target_include_directories(oscilloscope_interface_proto
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_link_libraries(oscilloscope_interface_proto PUBLIC protobuf::libprotobuf)
