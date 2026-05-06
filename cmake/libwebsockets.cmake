set(LWS_WITHOUT_TESTAPPS ON CACHE BOOL "" FORCE)
set(LWS_WITH_MINIMAL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LWS_WITHOUT_EXTENSIONS ON CACHE BOOL "" FORCE)
set(LWS_WITH_HTTP2 OFF CACHE BOOL "" FORCE)
set(LWS_WITH_HTTP_PROXY OFF CACHE BOOL "" FORCE)
set(LWS_WITH_SOCKS5 OFF CACHE BOOL "" FORCE)
set(LWS_IPV6 OFF CACHE BOOL "" FORCE)
set(LWS_WITH_SSL ON CACHE BOOL "" FORCE)
set(LWS_STATIC_PIC ON CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
        websockets
        GIT_REPOSITORY https://github.com/warmcat/libwebsockets.git
        GIT_TAG v4.5.2
)
FetchContent_MakeAvailable(websockets)

# Create pkg-config-style variables with "lws_" prefix
FetchContent_GetProperties(websockets)
if(websockets_POPULATED)

    # Mimic pkg_check_modules variable naming
    set(lws_INCLUDE_DIRS
            ${websockets_SOURCE_DIR}/include
            ${websockets_BINARY_DIR}
    )

    set(lws_LIBRARIES websockets)
    set(lws_LIBRARY_DIRS ${websockets_BINARY_DIR})

    set(lws_FOUND TRUE)
endif()
