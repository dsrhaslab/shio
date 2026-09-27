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
    && cmake ..

RUN cd control_plane/build \
    && cmake --build .

# Synthetic data plane.
COPY data_plane/synthetic_dp ./data_plane/synthetic_dp

RUN mkdir -p data_plane/synthetic_dp/build \
    && cd data_plane/synthetic_dp/build \
    && cmake .. \
    && cmake --build .

# Realistic data plane: PAIO, and PADLL on top of it.
ENV PAIO_DIR=/shio/data_plane/realistic_dp/paio_padll_dp/paio
ENV PADLL_DIR=/shio/data_plane/realistic_dp/paio_padll_dp/padll

# PAIO headers use uint64_t without including <cstdint>, which gcc 13 no longer pulls in
# transitively, so force-include it for PAIO and for PADLL (which includes PAIO's headers).
ENV PAIO_CXX_FLAGS="-include cstdint"

COPY data_plane/realistic_dp/paio_padll_dp ./data_plane/realistic_dp/paio_padll_dp

RUN mkdir -p ${PAIO_DIR}/build \
    && cd ${PAIO_DIR}/build \
    && cmake .. -DCMAKE_CXX_FLAGS="${PAIO_CXX_FLAGS}" \
    && cmake --build . -j "$(nproc)"

# PADLL finds libpaio through PAIO_LOCAL_PATH, which is hardcoded in its CMakeLists.txt (so it
# cannot be overridden with -D) and is rewritten here to point to PAIO's build directory. PAIO
# headers are found through CPATH.
ENV CPATH=${PAIO_DIR}/include
ENV LD_LIBRARY_PATH=${PAIO_DIR}/build

RUN sed -i 's|set(PAIO_LOCAL_PATH ".*")|set(PAIO_LOCAL_PATH "'"${PAIO_DIR}"'/build")|' \
        ${PADLL_DIR}/CMakeLists.txt \
    && grep -q "set(PAIO_LOCAL_PATH \"${PAIO_DIR}/build\")" ${PADLL_DIR}/CMakeLists.txt \
    && mkdir -p ${PADLL_DIR}/build \
    && cd ${PADLL_DIR}/build \
    && cmake .. -DCMAKE_CXX_FLAGS="${PAIO_CXX_FLAGS}" \
    && cmake --build . -j "$(nproc)"

ENV PATH_PADLL=${PADLL_DIR}/build

# Trace replayer and the collected traces. Each <app>/cN_merged.zip is extracted next to the
# zip, into <app>/cN_merged/.
RUN apt-get update \
    && apt-get install -y --no-install-recommends unzip \
    && rm -rf /var/lib/apt/lists/*

ENV TRACE_REPLAYER_DIR=/shio/data_plane/realistic_dp/trace_replayer

COPY data_plane/realistic_dp/trace_replayer ./data_plane/realistic_dp/trace_replayer

# Flags for building with gcc 13 / glibc without source changes:
#  -D_GNU_SOURCE          declares fopen64 (otherwise implicitly declared, truncating the FILE*)
#  -include sys/stat.h    declares mkdir
#  -U_FORTIFY_SOURCE      Ubuntu's default fortify rejects openat(O_CREAT) without a mode argument
RUN cd ${TRACE_REPLAYER_DIR} \
    && gcc -O2 -pthread -D_GNU_SOURCE -include sys/stat.h -U_FORTIFY_SOURCE \
        -o trace_replayer trace_replayer_file_cropping.c

RUN find ${TRACE_REPLAYER_DIR}/traces_collected -name '*.zip' \
        -execdir unzip -o -q {} \;

# Scripts (e.g., launch_hierarchy.sh). Copied last since they change often and do not affect the
# build layers above.
COPY local_scripts ./local_scripts
