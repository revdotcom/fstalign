# The image base (BASE_OS): resolute (Ubuntu 26.04, the default) or wolfi (Chainguard Wolfi), the bases of Rev's speech
# images (speechrec whisperx, OT, diarization). fstalign links ICU dynamically and ICU's soname changes with each
# distro, so a consumer takes the build of its own base. BASE_OS picks the OS image (base-<BASE_OS>).
ARG BASE_OS=resolute
FROM ubuntu:26.04 AS base-resolute
# Canonical's ubuntu:26.04 OCI image adds /usr/bin/pebble (the rocks service manager, Go, not dpkg-owned, so apt never
# patches it); nothing here uses it and its Go stdlib carries Critical/High CVEs.
RUN rm -f /usr/bin/pebble && rm -rf /var/lib/pebble
RUN echo "APT::Get::Assume-Yes \"true\";\nAPT::Get::allow \"true\";" | tee -a  /etc/apt/apt.conf.d/90_no_prompt && \
    echo "APT::Keep-Downloaded-Packages \"false\";" | tee -a  /etc/apt/apt.conf.d/91_no_cache && \
    apt-get update
# bash and ldd: the CI checks run in the image with them.
FROM cgr.dev/chainguard/wolfi-base:latest AS base-wolfi
RUN apk add --no-cache bash posix-libc-utils
FROM base-${BASE_OS} AS os-base
ARG BASE_OS
ENV DEBIAN_FRONTEND=noninteractive
# Stage 1: Build OpenFST 1.7.2 from source
FROM os-base AS openfst-builder

ARG OPENFST_VERSION=1.7.2
ARG JOBS=4

# Install build dependencies for OpenFST
RUN if [ "$BASE_OS" = wolfi ]; then apk add --no-cache gcc glibc-dev make ccache; exit; fi; \
    apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y --no-install-recommends \
    g++ \
    make \
    ccache \
    && rm -rf /var/lib/apt/lists/*

# Copy and build OpenFST from local tarball
WORKDIR /tmp
COPY ext/openfst-${OPENFST_VERSION}.tar.gz /tmp/
RUN --mount=type=cache,target=/root/.ccache,sharing=locked  \
    PATH=/usr/lib/ccache:${PATH} \
    tar -xzf openfst-${OPENFST_VERSION}.tar.gz && \
    cd openfst-${OPENFST_VERSION} && \
    # OpenFst 1.7.2 on newer compilers, as rev-kaldi's tools/Makefile patches it. GCC 14+: a raw pointer
    # can't be assigned to a unique_ptr. GCC 15+ (resolute, wolfi) checks uninstantiated template bodies: VectorHashBiTable's
    # copy constructor names a member that doesn't exist (later OpenFst releases use *table.selector_). Neither
    # changes code that older compilers build.
    sed -i 's/isymbols_ = \(impl\.isymbols_\)/isymbols_.reset(\1/' src/include/fst/fst.h && \
    sed -i 's/osymbols_ = \(impl\.osymbols_\)/osymbols_.reset(\1/' src/include/fst/fst.h && \
    sed -i '/[io]symbols_\.reset/s/: nullptr;/: nullptr);/' src/include/fst/fst.h && \
    sed -i 's/selector_(new S(table\.s_))/selector_(new S(*table.selector_))/' src/include/fst/bi-table.h && \
    ./configure --prefix=/opt/openfst --enable-shared --enable-static && \
    make -j${JOBS} && \
    make install && \
    cd .. && \
    rm -rf openfst-${OPENFST_VERSION} openfst-${OPENFST_VERSION}.tar.gz

# Stage 2: Build fstalign
FROM os-base

COPY --from=openfst-builder /opt/openfst /opt/openfst
ENV OPENFST_ROOT /opt/openfst

ARG JOBS=4

# Install runtime and build dependencies
RUN if [ "$BASE_OS" = wolfi ]; then apk add --no-cache cmake gcc glibc-dev make ccache icu-dev; exit; fi; \
    apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y --no-install-recommends \
    cmake \
    g++ \
    make \
    ccache \
    libicu-dev \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir /fstalign
COPY CMakeLists.txt /fstalign/CMakeLists.txt
COPY src /fstalign/src
COPY test /fstalign/test
COPY third-party /fstalign/third-party
COPY sample_data /fstalign/sample_data

WORKDIR /fstalign

# CMake 4 refuses the bundled jsoncpp's cmake_minimum_required below 3.5 unless this is set; CMake 3.x
# ignores it. As rev-asr-core's ci/async-asr/Dockerfile and revcoder's static ffmpeg build.
ENV CMAKE_POLICY_VERSION_MINIMUM=3.5

RUN --mount=type=cache,target=/root/.ccache,sharing=locked  \
    PATH=/usr/lib/ccache:${PATH} \
    mkdir -p /fstalign/build && \
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
