cmake_minimum_required(VERSION 3.25)
project(oscilloscope_interface VERSION 0.1.0 LANGUAGES CXX)

find_package(Protobuf REQUIRED)

# ── buf_connect_server generic proto ────────────────────────────────────────
add_library(buf_connect_server_proto STATIC)

protobuf_generate(
        TARGET    buf_connect_server_proto
        LANGUAGE  cpp
        PROTOS    ${CMAKE_CURRENT_SOURCE_DIR}/../buf_connect_server/proto/buf_connect_server.proto
        PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}
        OUT_VAR   BCS_PROTO_SRCS
)

target_include_directories(buf_connect_server_proto
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_link_libraries(buf_connect_server_proto PUBLIC protobuf::libprotobuf)

# ── oscilloscope device-specific proto ───────────────────────────────────────
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

target_link_libraries(oscilloscope_interface_proto
        PUBLIC
        protobuf::libprotobuf
        buf_connect_server_proto
)
