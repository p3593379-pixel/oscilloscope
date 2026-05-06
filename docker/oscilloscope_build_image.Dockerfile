ARG BUILD_OS
ARG TARGET_ARCH
FROM oscilloscope_build_image_base_$TARGET_ARCH:$BUILD_OS AS oscilloscope_build_image
ARG BUILD_OS
ARG TARGET_ARCH
COPY . /opt/oscilloscope

ENV TARGET_ARCH=${TARGET_ARCH}
ENV BUILD_OS=${BUILD_OS}

WORKDIR /opt/oscilloscope/oscilloscope_web
RUN npm install && npm run build

WORKDIR /opt/oscilloscope/control_panel_web
RUN npm install && npm run build

WORKDIR /opt/oscilloscope
#RUN if [ "$TARGET_ARCH" = "arm64" ]; then \
#    ls -la /opt/aarch64-sysroot/aarch64-libusb/include/ && \
#    ls -la /opt/aarch64-sysroot/aarch64-libusb/include/libusb-1.0 && \
#    ls -la /opt/aarch64-sysroot/aarch64-libusb/lib/ && \
#    cat /opt/aarch64-sysroot/aarch64-libusb/lib/pkgconfig/libusb-1.0.pc; \
#    fi
#
#RUN if [ "$TARGET_ARCH" = "arm64" ]; then \
#        ls -la /opt/aarch64-sysroot/aarch64-lws/lib/ \
#      && ls -la /opt/aarch64-sysroot/aarch64-openssl/lib/ \
#      && ls -la /opt/aarch64-sysroot/aarch64-cjson/lib/ \
#      && ls -la /opt/aarch64-sysroot/aarch64-libusb/lib/ \
#      && ls -la /opt/aarch64-sysroot/aarch64-libudev/lib/; \
#    fi
ENV AARCH64_SYSROOT=/opt/aarch64-sysroot
ENV OPENSSL_AARCH64=/opt/aarch64-sysroot/aarch64-openssl
ENV SQLITE3_AARCH64=/opt/aarch64-sysroot/aarch64-sqlite3
ENV PROTOBUF_AARCH64=/opt/aarch64-sysroot/aarch64-protobuf
ENV NGHTTP2_AARCH64=/opt/aarch64-sysroot/aarch64-nghttp2
ENV ARGON2_AARCH64=/opt/aarch64-sysroot/aarch64-argon2
ENV UUID_AARCH64=/opt/aarch64-sysroot/aarch64-uuid

RUN ls /opt/aarch64-sysroot/aarch64-protobuf/lib/cmake/ && \
    ls /opt/aarch64-sysroot/aarch64-protobuf/lib/cmake/absl/ | head -5

RUN if [ "$TARGET_ARCH" = "arm64" ]; then \
     cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE=/opt/oscilloscope/cmake/aarch64-toolchain.cmake \
    -DEMBEDDED_SYSTEM=1 \
    -DCMAKE_TOOLCHAIN_FILE=/opt/aarch64-toolchain.cmake \
    -DOPENSSL_ROOT_DIR=/opt/aarch64-sysroot/aarch64-openssl \
    -DSQLite3_ROOT=/opt/aarch64-sysroot/aarch64-sqlite3 \
    -DProtobuf_ROOT=/opt/aarch64-sysroot/aarch64-protobuf \
    -DNGHTTP2_ROOT=/opt/aarch64-sysroot/aarch64-nghttp2 \
    -DARGON2_ROOT=/opt/aarch64-sysroot/aarch64-argon2 \
    -DUUID_ROOT=/opt/aarch64-sysroot/aarch64-uuid \
    -Dprotobuf_PROTOC_EXECUTABLE=/opt/host-protobuf/bin/protoc; \
    else \
    cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug; \
    fi

WORKDIR /opt/oscilloscope/build
RUN ninja -j $(nproc)
RUN #cpack -GDEB && mv _CPack_Packages/Linux/DEB/*.deb ..
