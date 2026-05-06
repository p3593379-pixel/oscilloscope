include(FetchContent)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE INTERNAL "")
set(SPDLOG_BUILD_TESTS OFF CACHE INTERNAL "")
FetchContent_Declare(spdlog
        URL http://sensor-dist.t8.ru/distr/spdlog-1.11.0.tar.gz
        BUILD_ALWAYS OFF
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_GetProperties(spdlog)
if(NOT spdlog_POPULATED)
    FetchContent_Populate(spdlog)
    add_subdirectory(${spdlog_SOURCE_DIR} ${spdlog_BINARY_DIR})
endif()