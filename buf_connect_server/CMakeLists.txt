# FILE: buf_connect_server/CMakeLists.txt
cmake_minimum_required(VERSION 3.25)
project(buf_connect_server VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/FetchDependencies.cmake)

add_library(buf_connect_server STATIC
        src/connect/frame_codec.cpp
        src/connect/request.cpp
        src/connect/response_writer.cpp
        src/connect/protocol.cpp
        src/http2/listener.cpp
        src/http2/router.cpp
        src/auth/jwt.cpp
        src/auth/stream_token.cpp
        src/auth/user_store.cpp
        src/auth/middleware.cpp
        src/session/session_manager.cpp
        src/config/config_loader.cpp
        src/config/config_server.cpp
        src/transport/control_plane.cpp
        src/transport/data_plane.cpp
        src/server.cpp
        # ── Migrated service handlers ──
        src/services/auth_handler.cpp
#        src/services/session_handler.cpp
#        src/services/admin_handler.cpp
)

add_library(buf_connect_server_proto_internal STATIC)

protobuf_generate(
        TARGET     buf_connect_server_proto_internal
        LANGUAGE   cpp
        PROTOS     ${CMAKE_CURRENT_SOURCE_DIR}/proto/buf_connect_server.proto
        PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}
        OUT_VAR    BCS_PROTO_SRCS
)

target_include_directories(buf_connect_server_proto_internal
        PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(buf_connect_server_proto_internal PUBLIC protobuf::libprotobuf)

target_include_directories(buf_connect_server
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        ${CMAKE_BINARY_DIR}/_deps/argon2-src/include
        ${CMAKE_BINARY_DIR}/_deps/nghttp2-src/lib/includes
        ${CMAKE_BINARY_DIR}/_deps/nghttp2-build/lib/includes
        # buf_connect_server.pb.h lives here (internal codegen)
        ${CMAKE_CURRENT_BINARY_DIR}/proto
        ${CMAKE_BINARY_DIR}/oscilloscope_interface
        PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_link_directories(buf_connect_server PRIVATE
        ${CMAKE_BINARY_DIR}/_deps/argon2-src
)

target_link_libraries(buf_connect_server
    PUBLIC
        buf_connect_server_proto_internal
        uuid
        argon2
        nghttp2
        OpenSSL::SSL
        OpenSSL::Crypto
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        sqlite_orm::sqlite_orm
        SQLite::SQLite3
        protobuf::libprotobuf
        jwt-cpp::jwt-cpp
        httplib::httplib
)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
