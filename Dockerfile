FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Build tools. gRPC, gflags, spdlog and yaml-cpp are downloaded by CMake (FetchContent), so git
# and CA certificates are needed at configure time.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        ca-certificates \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*

COPY . .

WORKDIR /control_plane

# Configure and compile. Headers include "_deps/grpc-src/...", which resolves relative to this
# build directory.
RUN mkdir -p build \
    && cd build \
    && cmake .. \
    && cmake --build . 

WORKDIR /shio/build

