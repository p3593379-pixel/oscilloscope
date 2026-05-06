# FILE: oscilloscope/CMakeLists.txt
cmake_minimum_required(VERSION 3.25)
project(oscilloscope VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build testing support
option(BUILD_TESTING "Build test suite" OFF)

find_program(NPM_EXECUTABLE npm)

if(NPM_EXECUTABLE)
    set(OSCILLOSCOPE_FRONTEND_SOURCE_DIR ${CMAKE_SOURCE_DIR}/oscilloscope_web)
    set(OSCILLOSCOPE_FRONTEND_DIST_DIR ${CMAKE_BINARY_DIR}/static/oscilloscope_web)
    add_compile_definitions(FRONTEND_DIST_DIR="${OSCILLOSCOPE_FRONTEND_DIST_DIR}")
    add_custom_target(build_oscilloscope_frontend
            COMMAND ${CMAKE_COMMAND} -E echo "Installing frontend dependencies..."
            COMMAND ${NPM_EXECUTABLE} install
            COMMAND ${CMAKE_COMMAND} -E echo "Building oscilloscope frontend..."
            COMMAND ${NPM_EXECUTABLE} run build
            WORKING_DIRECTORY ${FRONTEND_SOURCE_DIR}
            COMMENT "Building oscilloscope frontend"
            VERBATIM
    )

    set(CONTROL_PANEL_FRONTEND_SOURCE_DIR ${CMAKE_SOURCE_DIR}/control_panel_web)
    set(CONTROL_PANEL_FRONTEND_DIST_DIR ${CMAKE_BINARY_DIR}/static/oscilloscope_web)
    add_compile_definitions(FRONTEND_DIST_DIR="${CONTROL_PANEL_FRONTEND_DIST_DIR}")
    add_custom_target(build_control_panel_frontend
            COMMAND ${CMAKE_COMMAND} -E echo "Installing frontend dependencies..."
            COMMAND ${NPM_EXECUTABLE} install
            COMMAND ${CMAKE_COMMAND} -E echo "Building control panel frontend..."
            COMMAND ${NPM_EXECUTABLE} run build
            WORKING_DIRECTORY ${FRONTEND_SOURCE_DIR}
            COMMENT "Building control panel frontend"
            VERBATIM
    )

    add_custom_target(clean_frontend
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${FRONTEND_DIST_DIR}
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${FRONTEND_SOURCE_DIR}/node_modules
            COMMENT "Cleaning frontend build artifacts"
    )
else()
    message(WARNING "npm not found - frontend must be built manually")
    message(WARNING "  cd web-react && npm install && npm run build")
endif()

configure_file(cmake/version/version.hpp.in version.hpp @ONLY)

#install(TARGETS oscilloscope
#        RUNTIME DESTINATION bin
#        COMPONENT application)
#
#install(DIRECTORY ${CMAKE_BINARY_DIR}/static/oscilloscope_web
#        DESTINATION /usr/share/oscilloscope/static/oscilloscope_web
#        COMPONENT application
#        PATTERN "node_modules" EXCLUDE
#        PATTERN ".git" EXCLUDE)
#
#install(DIRECTORY ${CMAKE_BINARY_DIR}/static/control_panel_web
#        DESTINATION /usr/share/oscilloscope/static/control_panel_web
#        COMPONENT application
#        PATTERN "node_modules" EXCLUDE
#        PATTERN ".git" EXCLUDE)

add_subdirectory(oscilloscope_interface)
add_subdirectory(buf_connect_server)
add_subdirectory(oscilloscope_backend)
