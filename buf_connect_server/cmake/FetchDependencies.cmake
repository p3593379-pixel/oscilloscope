include(FetchContent)

# ─────────────────────────────────────────────────────────────────────────────
# Sysroot prefix helpers
# ─────────────────────────────────────────────────────────────────────────────
if(CMAKE_CROSSCOMPILING)
    set(_SYSROOT_PREFIX /opt/aarch64-sysroot)
    set(_ARCH_TAG       aarch64)
#else()
#    set(_SYSROOT_PREFIX /opt/x86)
#    set(_ARCH_TAG       x86)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# OpenSSL — pre-built in sysroot, found via find_package
# ─────────────────────────────────────────────────────────────────────────────
set(OPENSSL_ROOT_DIR    ${_SYSROOT_PREFIX}/${_ARCH_TAG}-openssl)
set(OPENSSL_USE_STATIC_LIBS ON)
find_package(OpenSSL REQUIRED)

# ─────────────────────────────────────────────────────────────────────────────
# SQLite3 — pre-built in sysroot
# ─────────────────────────────────────────────────────────────────────────────
# SQLite3 — force cache variables so sub-project find_package calls also resolve correctly
if (CMAKE_CROSSCOMPILING)
    set(SQLite3_INCLUDE_DIR
            "${_SYSROOT_PREFIX}/${_ARCH_TAG}-sqlite3/include"
            CACHE PATH "" FORCE
    )
    set(SQLite3_LIBRARY
            "${_SYSROOT_PREFIX}/${_ARCH_TAG}-sqlite3/lib/libsqlite3.a"
            CACHE FILEPATH "" FORCE
    )
endif ()
find_package(SQLite3 REQUIRED)

# ─────────────────────────────────────────────────────────────────────────────
# Protobuf — pre-built in sysroot, host protoc patched after find_package
# ─────────────────────────────────────────────────────────────────────────────
if (CMAKE_CROSSCOMPILING)
    set(Protobuf_ROOT       ${_SYSROOT_PREFIX}/${_ARCH_TAG}-protobuf)
    list(APPEND CMAKE_PREFIX_PATH ${Protobuf_ROOT})

    find_package(Protobuf REQUIRED CONFIG
            HINTS ${Protobuf_ROOT}
            NO_DEFAULT_PATH
    )
    find_package(absl REQUIRED CONFIG
            HINTS ${Protobuf_ROOT}
            NO_DEFAULT_PATH
    )
else ()
    find_package(Protobuf REQUIRED)
endif ()

# Fix protobuf::protoc to use host-native binary when cross-compiling
# (CONFIG mode ignores Protobuf_PROTOC_EXECUTABLE — protobuf bug #14576)
if(CMAKE_CROSSCOMPILING AND TARGET protobuf::protoc)
    set_target_properties(protobuf::protoc PROPERTIES
            IMPORTED_LOCATION             /opt/host-protobuf/bin/protoc
            IMPORTED_LOCATION_RELEASE     /opt/host-protobuf/bin/protoc
            IMPORTED_LOCATION_DEBUG       /opt/host-protobuf/bin/protoc
            IMPORTED_LOCATION_NOCONFIG    /opt/host-protobuf/bin/protoc
    )
    message(STATUS "Cross-compiling: patched protobuf::protoc -> /opt/host-protobuf/bin/protoc")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# nghttp2 — pre-built static lib in sysroot (NOT via FetchContent — it would
#            rebuild as shared and ignore our static sysroot version)
# ─────────────────────────────────────────────────────────────────────────────
if (CMAKE_CROSSCOMPILING)
set(_NGHTTP2_ROOT ${_SYSROOT_PREFIX}/${_ARCH_TAG}-nghttp2)
add_library(nghttp2::nghttp2 STATIC IMPORTED GLOBAL)
set_target_properties(nghttp2::nghttp2 PROPERTIES
        IMPORTED_LOCATION             "${_NGHTTP2_ROOT}/lib/libnghttp2.a"
        INTERFACE_INCLUDE_DIRECTORIES "${_NGHTTP2_ROOT}/include"
)
else ()
    FetchContent_Declare(nghttp2
            GIT_REPOSITORY https://github.com/nghttp2/nghttp2.git
            GIT_TAG        v1.61.0
    )
    set(ENABLE_LIB_ONLY ON CACHE BOOL "" FORCE)
    set(ENABLE_SHARED_LIB OFF CACHE BOOL "" FORCE)
    set(ENABLE_STATIC_LIB ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(nghttp2)
endif ()
# ─────────────────────────────────────────────────────────────────────────────
# argon2 — pre-built static lib in sysroot (no CMakeLists.txt in source tree)
# ─────────────────────────────────────────────────────────────────────────────
if (CMAKE_CROSSCOMPILING)
    set(_ARGON2_ROOT ${_SYSROOT_PREFIX}/${_ARCH_TAG}-argon2)
    add_library(argon2 STATIC IMPORTED GLOBAL)
    set_target_properties(argon2 PROPERTIES
            IMPORTED_LOCATION             "${_ARGON2_ROOT}/lib/libargon2.a"
            INTERFACE_INCLUDE_DIRECTORIES "${_ARGON2_ROOT}/include"
    )
else ()
    FetchContent_Declare(
            argon2
            GIT_REPOSITORY https://github.com/P-H-C/phc-winner-argon2.git
            GIT_TAG        20190702
    )
    FetchContent_MakeAvailable(argon2)

endif ()

# ─────────────────────────────────────────────────────────────────────────────
# uuid — pre-built static lib in sysroot (from util-linux)
# ─────────────────────────────────────────────────────────────────────────────
if (CMAKE_CROSSCOMPILING)
    set(_UUID_ROOT ${_SYSROOT_PREFIX}/${_ARCH_TAG}-uuid)
    add_library(uuid STATIC IMPORTED GLOBAL)
    set_target_properties(uuid PROPERTIES
            IMPORTED_LOCATION             "${_UUID_ROOT}/lib/libuuid.a"
            INTERFACE_INCLUDE_DIRECTORIES "${_UUID_ROOT}/include"
    )
else ()
    FetchContent_Declare(cpp_httplib
            GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
            GIT_TAG        master
    )
    FetchContent_MakeAvailable(cpp_httplib)
endif ()

# ─────────────────────────────────────────────────────────────────────────────
# spdlog — FetchContent, forced static + header-only to avoid .so
# ─────────────────────────────────────────────────────────────────────────────
set(SPDLOG_BUILD_SHARED  OFF CACHE BOOL "" FORCE)
set(SPDLOG_HEADER_ONLY   ON  CACHE BOOL "" FORCE)
FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.13.0
)
FetchContent_MakeAvailable(spdlog)

# ─────────────────────────────────────────────────────────────────────────────
# nlohmann/json — header-only, FetchContent is fine
# ─────────────────────────────────────────────────────────────────────────────
FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# ─────────────────────────────────────────────────────────────────────────────
# sqlite_orm — header-only, FetchContent is fine
# ─────────────────────────────────────────────────────────────────────────────
FetchContent_Declare(sqlite_orm
        GIT_REPOSITORY https://github.com/fnc12/sqlite_orm.git
        GIT_TAG        v1.8.2
)
FetchContent_MakeAvailable(sqlite_orm)

# ─────────────────────────────────────────────────────────────────────────────
# jwt-cpp — header-only, FetchContent is fine
# ─────────────────────────────────────────────────────────────────────────────
set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(jwt_cpp
        GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
        GIT_TAG        master
)
FetchContent_MakeAvailable(jwt_cpp)

# ─────────────────────────────────────────────────────────────────────────────
# cpp-httplib — header-only, FetchContent is fine
# ─────────────────────────────────────────────────────────────────────────────
FetchContent_Declare(cpp_httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG        master
)
FetchContent_MakeAvailable(cpp_httplib)

# ─────────────────────────────────────────────────────────────────────────────
# GoogleTest — only for host builds (no point cross-compiling tests)
# ─────────────────────────────────────────────────────────────────────────────
if(NOT CMAKE_CROSSCOMPILING)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
endif()
