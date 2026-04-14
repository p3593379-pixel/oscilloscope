# FILE: oscilloscope_backend/CMakeLists.txt
cmake_minimum_required(VERSION 3.25)
project(oscilloscope_backend VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(oscilloscope_backend
        src/main.cpp
        src/services/oscilloscope_service_impl.cpp
        src/services/archive_service_impl.cpp
        src/hardware/adc_reader.cpp
        src/hardware/dat_file_reader.cpp
)

target_link_directories(oscilloscope_backend PRIVATE
        ${CMAKE_BINARY_DIR}/_deps/argon2-src
)

target_link_libraries(oscilloscope_backend
        PRIVATE
        argon2
        buf_connect_server
        oscilloscope_interface_proto
)

target_include_directories(oscilloscope_backend
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
        PUBLIC ${CMAKE_BINARY_DIR}/oscilloscope_interface/
)
