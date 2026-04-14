# FILE: oscilloscope/CMakeLists.txt
cmake_minimum_required(VERSION 3.25)
project(oscilloscope VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build testing support
option(BUILD_TESTING "Build test suite" ON)

add_subdirectory(oscilloscope_interface)
add_subdirectory(buf_connect_server)
add_subdirectory(oscilloscope_backend)
