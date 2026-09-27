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

WORKDIR /shio

# Control plane. Copied and built on its own so that data plane changes do not invalidate the
# (slow) gRPC build layer.
COPY control_plane ./control_plane

# Configure and compile. Headers include "_deps/grpc-src/...", which resolves relative to this
# build directory.
RUN mkdir -p control_plane/build \
    && cd control_plane/build \
    && cmake .. \
    && cmake --build .

# Data plane.
COPY data_plane ./data_plane

RUN mkdir -p data_plane/synthetic_data_plane/build \
    && cd data_plane/synthetic_data_plane/build \
    && cmake .. \
    && cmake --build .
