cmake_minimum_required(VERSION 3.25)
project(oscilloscope_interface VERSION 0.1.0 LANGUAGES CXX)
if (CMAKE_CROSSCOMPILING)
    set(Protobuf_PROTOC_EXECUTABLE /opt/host-protobuf/bin/protoc CACHE FILEPATH "" FORCE)
endif ()
find_package(Protobuf REQUIRED)

# ── oscilloscope device-specific proto ───────────────────────────────────────
add_library(buf_connect_server_proto_internal STATIC)

protobuf_generate(
        TARGET    buf_connect_server_proto_internal
        LANGUAGE  cpp
        PROTOS    buf_connect_server.proto
        OUT_VAR   PROTO_GENERATED_SRCS
)

target_include_directories(buf_connect_server_proto_internal
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_link_libraries(buf_connect_server_proto_internal
        PUBLIC
        protobuf::libprotobuf
)
