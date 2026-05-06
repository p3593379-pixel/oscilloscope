include(FetchContent)
FetchContent_Declare(
        cjson
        GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
        GIT_TAG v1.7.18
)

set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(cjson)

# Create pkg-config-style variables
FetchContent_GetProperties(cjson)
if(cjson_POPULATED)
    set(cjson_INCLUDE_DIRS ${cjson_SOURCE_DIR})
    set(cjson_LIBRARIES cjson)
    set(cjson_STATIC_LIBRARIES cjson)
    set(cjson_LIBRARY_DIRS ${cjson_BINARY_DIR})
    set(cjson_FOUND TRUE)
endif()
