ARG BASE_IMAGE="docker.t8.ru/ubuntu:22.04"
FROM ${BASE_IMAGE} AS oscilloscope_build_image_base
WORKDIR /opt/
ENV DEBIAN_FRONTEND=noninteractive
ARG BUILD_OS
ARG TARGET_ARCH
ENV NVM_DIR=/root/.nvm
ARG NODE_VERSION=22.12.0

RUN ldd --version
RUN apt-get upgrade
RUN apt-get update
RUN apt-get install -y build-essential
RUN apt-get install -y git
RUN apt-get install -y wget
RUN apt-get install -y ninja-build
RUN apt-get install -y pkg-config
RUN apt-get install -y autoconf
RUN apt-get install -y automake
RUN apt-get install -y curl
RUN apt-get install -y libtool
RUN apt-get install -y meson
RUN apt-get update && apt-get install -y curl && \
    curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.3/install.sh | bash && \
    . "$NVM_DIR/nvm.sh" && \
    . "$NVM_DIR/bash_completion" && \
    nvm install ${NODE_VERSION} && \
    nvm use ${NODE_VERSION} && \
    nvm alias default ${NODE_VERSION} && \
    node --version && \
    npm --version

ENV PATH="/root/.nvm/versions/node/v${NODE_VERSION}/bin:${PATH}"
RUN npm --version && node --version

RUN apt-get install -y pkg-config
RUN apt-get install -y gperf

RUN wget --quiet http://sensor-dist.t8.ru/distr/cmake-3.25.3.tar.gz \
    && tar -xzf cmake-3.25.3.tar.gz \
    && cd cmake-3.25.3/ \
    && ./bootstrap --parallel=$(nproc) -- -DCMAKE_USE_OPENSSL=OFF \
    && make -j$(nproc) && make install

RUN if [ "$TARGET_ARCH" = "arm64" ]; then \
        apt-get install -y gcc-aarch64-linux-gnu && \
        apt-get install -y g++-aarch64-linux-gnu; \
    fi

RUN mkdir -p /opt/aarch64-sysroot
COPY ./cmake/aarch64-toolchain.cmake /opt/aarch64-toolchain.cmake
RUN ls -la /opt/aarch64-toolchain.cmake

WORKDIR /build-deps

# ────────────────────────────────────────────────────────────────────────────
# 1. OpenSSL (required by nghttp2, jwt-cpp, cpp-httplib)
# ────────────────────────────────────────────────────────────────────────────
RUN wget https://www.openssl.org/source/openssl-3.0.13.tar.gz && \
    tar -xzf openssl-3.0.13.tar.gz && \
    cd openssl-3.0.13 && \
    if [ "$TARGET_ARCH" = "arm64" ]; then \
        ./Configure linux-aarch64 \
            --prefix=/opt/aarch64-sysroot/aarch64-openssl \
            --cross-compile-prefix=aarch64-linux-gnu- \
            no-shared no-module no-tests -static; \
    else \
        ./Configure linux-x86_64 \
            --prefix=/opt/x86-openssl \
            --libdir=lib \
            no-shared no-module no-tests -static; \
    fi && \
    make -j$(nproc) && \
    make install_sw && \
    cd /build-deps && rm -rf openssl-*

# ────────────────────────────────────────────────────────────────────────────
# 2. SQLite3 (find_package(SQLite3 REQUIRED))
# ────────────────────────────────────────────────────────────────────────────
RUN wget https://www.sqlite.org/2024/sqlite-autoconf-3450200.tar.gz && \
    tar -xzf sqlite-autoconf-3450200.tar.gz && \
    cd sqlite-autoconf-3450200 && \
    if [ "$TARGET_ARCH" = "arm64" ]; then \
        ./configure \
            --host=aarch64-linux-gnu \
            --build=x86_64-linux-gnu \
            --prefix=/opt/aarch64-sysroot/aarch64-sqlite3 \
            CC=aarch64-linux-gnu-gcc \
            CXX=aarch64-linux-gnu-g++ \
            --disable-shared \
            --enable-static; \
    else \
        ./configure \
            --prefix=/opt/x86-sqlite3 \
            --disable-shared \
            --enable-static; \
    fi && \
    make -j$(nproc) && \
    make install && \
    cd /build-deps && rm -rf sqlite-autoconf-*

# ────────────────────────────────────────────────────────────────────────────
# 3. Protobuf (find_package(Protobuf REQUIRED))
#    Build host protoc first, then cross-compile the runtime library.
# ────────────────────────────────────────────────────────────────────────────
RUN git clone --recurse-submodules --depth=1 --branch v25.3 \
        https://github.com/protocolbuffers/protobuf.git protobuf-25.3 && \
    # Build host protoc
    cd protobuf-25.3 && \
    cmake -B build-host -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/host-protobuf \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_BUILD_SHARED_LIBS=OFF \
        -Dprotobuf_ABSL_PROVIDER=module && \
    cmake --build build-host -j$(nproc) --target protoc && \
    cmake --install build-host --component protoc && \
    # Cross-compile runtime for aarch64
    if [ "$TARGET_ARCH" = "arm64" ]; then \
        cmake -B build-cross -S . \
            -DCMAKE_TOOLCHAIN_FILE=/opt/aarch64-toolchain.cmake \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX=/opt/aarch64-sysroot/aarch64-protobuf \
            -Dprotobuf_BUILD_TESTS=OFF \
            -Dprotobuf_BUILD_SHARED_LIBS=OFF \
            -Dprotobuf_BUILD_PROTOC_BINARIES=OFF \
            -Dprotobuf_ABSL_PROVIDER=module && \
        cmake --build build-cross -j$(nproc) && \
        cmake --install build-cross; \
    fi && \
    cd /build-deps && rm -rf protobuf-25.3

ENV PATH="/opt/host-protobuf/bin:${PATH}"

# ────────────────────────────────────────────────────────────────────────────
# 4. nghttp2 (static, lib-only — FetchContent sets these flags but a
#    pre-built sysroot copy avoids repeated compilation per project build)
# ────────────────────────────────────────────────────────────────────────────
RUN wget https://github.com/nghttp2/nghttp2/releases/download/v1.61.0/nghttp2-1.61.0.tar.gz && \
    tar -xzf nghttp2-1.61.0.tar.gz && \
    cd nghttp2-1.61.0 && \
    if [ "$TARGET_ARCH" = "arm64" ]; then \
        cmake -B build -S . \
            -DCMAKE_TOOLCHAIN_FILE=/opt/aarch64-toolchain.cmake \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX=/opt/aarch64-sysroot/aarch64-nghttp2 \
            -DENABLE_LIB_ONLY=ON \
            -DBUILD_SHARED_LIBS=OFF \
            -DBUILD_STATIC_LIBS=ON \
            -DENABLE_HTTP3=OFF \
            -DWITH_JEMALLOC=OFF \
            -DWITH_LIBXML2=OFF \
            -DWITH_JANSSON=OFF \
            -DOPENSSL_ROOT_DIR=/opt/aarch64-sysroot/aarch64-openssl \
            -DOPENSSL_INCLUDE_DIR=/opt/aarch64-sysroot/aarch64-openssl/include \
            -DOPENSSL_CRYPTO_LIBRARY=/opt/aarch64-sysroot/aarch64-openssl/lib/libcrypto.a \
            -DOPENSSL_SSL_LIBRARY=/opt/aarch64-sysroot/aarch64-openssl/lib/libssl.a; \
    else \
        cmake -B build -S . \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX=/opt/x86-nghttp2 \
            -DENABLE_LIB_ONLY=ON \
            -DBUILD_SHARED_LIBS=OFF \
            -DBUILD_STATIC_LIBS=ON \
            -DENABLE_HTTP3=OFF \
            -DWITH_JEMALLOC=OFF \
            -DWITH_LIBXML2=OFF \
            -DWITH_JANSSON=OFF \
            -DOPENSSL_ROOT_DIR=/opt/x86-openssl; \
    fi && \
    cmake --build build -j$(nproc) && \
    cmake --install build && \
    cd /build-deps && rm -rf nghttp2-*

# ────────────────────────────────────────────────────────────────────────────
# 5. Argon2 (no CMake — plain Makefile with cross-compile overrides)
# ────────────────────────────────────────────────────────────────────────────
RUN git clone https://github.com/P-H-C/phc-winner-argon2.git argon2 && \
    cd argon2 && \
    git checkout 20190702 && \
    if [ "$TARGET_ARCH" = "arm64" ]; then \
        CC=aarch64-linux-gnu-gcc; \
        AR=aarch64-linux-gnu-ar; \
        RANLIB=aarch64-linux-gnu-ranlib; \
        PREFIX=/opt/aarch64-sysroot/aarch64-argon2; \
    else \
        CC=gcc; \
        AR=ar; \
        RANLIB=ranlib; \
        PREFIX=/opt/x86-argon2; \
    fi && \
    # Compile all sources manually, bypassing the broken Makefile ARCH detection
    ${CC} -std=c89 -O3 -Wall -Iinclude -Isrc -pthread -c src/argon2.c    -o src/argon2.o && \
    ${CC} -std=c89 -O3 -Wall -Iinclude -Isrc -pthread -c src/core.c      -o src/core.o && \
    ${CC} -std=c89 -O3 -Wall -Iinclude -Isrc -pthread -c src/blake2/blake2b.c -o src/blake2/blake2b.o && \
    ${CC} -std=c89 -O3 -Wall -Iinclude -Isrc -pthread -c src/thread.c    -o src/thread.o && \
    ${CC} -std=c89 -O3 -Wall -Iinclude -Isrc -pthread -c src/encoding.c  -o src/encoding.o && \
    ${CC} -std=c89 -O3 -Wall -Iinclude -Isrc -pthread -c src/ref.c       -o src/ref.o && \
    # Link the static library
    ${AR} rcs libargon2.a \
        src/argon2.o src/core.o src/blake2/blake2b.o \
        src/thread.o src/encoding.o src/ref.o && \
    ${RANLIB} libargon2.a && \
    # Install headers and static lib manually
    mkdir -p ${PREFIX}/include ${PREFIX}/lib && \
    cp include/argon2.h ${PREFIX}/include/ && \
    cp libargon2.a ${PREFIX}/lib/ && \
    cd /build-deps && rm -rf argon2

RUN wget https://mirrors.edge.kernel.org/pub/linux/utils/util-linux/v2.39/util-linux-2.39.tar.gz && \
    tar -xzf util-linux-2.39.tar.gz && \
    cd util-linux-2.39 && \
    if [ "$TARGET_ARCH" = "arm64" ]; then \
        ./configure \
            --host=aarch64-linux-gnu \
            --build=x86_64-linux-gnu \
            --prefix=/opt/aarch64-sysroot/aarch64-uuid \
            CC=aarch64-linux-gnu-gcc \
            CXX=aarch64-linux-gnu-g++ \
            --disable-all-programs \
            --enable-libuuid \
            --disable-shared \
            --enable-static; \
    else \
        ./configure \
            --prefix=/opt/x86-uuid \
            --disable-all-programs \
            --enable-libuuid \
            --disable-shared \
            --enable-static; \
    fi && \
    make -j$(nproc) && \
    make install && \
    cd /build-deps && rm -rf util-linux-*

# ────────────────────────────────────────────────────────────────────────────
# Environment hints for downstream CMake builds
# ────────────────────────────────────────────────────────────────────────────
ENV AARCH64_SYSROOT=/opt/aarch64-sysroot
ENV OPENSSL_AARCH64=/opt/aarch64-sysroot/aarch64-openssl
ENV SQLITE3_AARCH64=/opt/aarch64-sysroot/aarch64-sqlite3
ENV PROTOBUF_AARCH64=/opt/aarch64-sysroot/aarch64-protobuf
ENV NGHTTP2_AARCH64=/opt/aarch64-sysroot/aarch64-nghttp2
ENV ARGON2_AARCH64=/opt/aarch64-sysroot/aarch64-argon2
ENV UUID_AARCH64=/opt/aarch64-sysroot/aarch64-uuid
