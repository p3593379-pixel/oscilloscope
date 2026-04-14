cmake_minimum_required(VERSION 3.25)
project(buf_connect_server_tests)

add_executable(buf_connect_server_tests
    connect/test_frame_codec.cpp
    connect/test_response_writer.cpp
    auth/test_jwt.cpp
    auth/test_stream_token.cpp
    auth/test_user_store.cpp
    session/test_session_manager.cpp
    config/test_config_loader.cpp
    integration/test_unary_rpc.cpp
    integration/test_streaming_rpc.cpp
)

target_link_directories(buf_connect_server_tests PRIVATE
        ${CMAKE_BINARY_DIR}/_deps/argon2-src
        ${CMAKE_BINARY_DIR}/buf_connect_server
)


target_link_libraries(buf_connect_server_tests
        PUBLIC
        buf_connect_server
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(buf_connect_server_tests)
