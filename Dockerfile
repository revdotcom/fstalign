# Stage 1: Build OpenFST 1.7.2 from source
FROM debian:bookworm as openfst-builder

ARG OPENFST_VERSION=1.7.2
ARG JOBS=4

# Install build dependencies for OpenFST
RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y --no-install-recommends \
    g++ \
    make \
    && rm -rf /var/lib/apt/lists/*

# Copy and build OpenFST from local tarball
WORKDIR /tmp
COPY ext/openfst-${OPENFST_VERSION}.tar.gz /tmp/
RUN tar -xzf openfst-${OPENFST_VERSION}.tar.gz && \
    cd openfst-${OPENFST_VERSION} && \
    ./configure --prefix=/opt/openfst --enable-shared --enable-static && \
    make -j${JOBS} && \
    make install && \
    cd .. && \
    rm -rf openfst-${OPENFST_VERSION} openfst-${OPENFST_VERSION}.tar.gz

# Stage 2: Build fstalign
FROM debian:bookworm

COPY --from=openfst-builder /opt/openfst /opt/openfst
ENV OPENFST_ROOT /opt/openfst

ARG JOBS=4

# Install runtime and build dependencies
RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y --no-install-recommends \
    cmake \
    g++ \
    make \
    libicu-dev \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir /fstalign
COPY CMakeLists.txt /fstalign/CMakeLists.txt
COPY src /fstalign/src
COPY test /fstalign/test
COPY third-party /fstalign/third-party
COPY sample_data /fstalign/sample_data

WORKDIR /fstalign

RUN mkdir -p /fstalign/build && \
    cd /fstalign/build && \
    rm -rf * && \
    cmake .. -DOPENFST_ROOT="${OPENFST_ROOT}" -DDYNAMIC_OPENFST=OFF && \
    make -j${JOBS} VERBOSE=1 && \
    mkdir -p /fstalign/bin && \
    cp /fstalign/build/fstalign /fstalign/bin && \
    strip /fstalign/bin/*

COPY tools /fstalign/tools

ENV PATH \
    /fstalign/bin/:\
    $PATH
