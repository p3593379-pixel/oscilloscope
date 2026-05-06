set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_CROSSCOMPILING ON)

# Specify cross-compilers
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Set sysroot where all our cross-compiled libraries live
set(CMAKE_SYSROOT /opt/aarch64-sysroot)
set(CMAKE_FIND_ROOT_PATH /opt/aarch64-sysroot)

# Search programs on host, libraries/includes only in target
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

set(HOST_PROTOC /opt/host-protobuf/bin/protoc)

macro(patch_protoc_target)
    if(TARGET protobuf::protoc)
        set_target_properties(protobuf::protoc PROPERTIES
                IMPORTED_LOCATION           ${HOST_PROTOC}
                IMPORTED_LOCATION_RELEASE   ${HOST_PROTOC}
                IMPORTED_LOCATION_DEBUG     ${HOST_PROTOC}
                IMPORTED_LOCATION_NOCONFIG  ${HOST_PROTOC}
        )
    endif()
endmacro()

cmake_language(DEFER CALL patch_protoc_target)
