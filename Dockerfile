# The image base, by codename (AI-611): bookworm, trixie, noble or resolute, the bases of Rev's speech images
# (speechrec whisperx, OT, diarization), resolute by default; bullseye is dropped. fstalign links ICU dynamically and ICU's soname
# changes with each distro, so a consumer takes the build of its own base. DEBIAN_BASE picks the OS image
# (base-<codename>); the name is kept for the Ubuntu ones too, as in speechrec.
ARG DEBIAN_BASE=resolute
FROM debian:bookworm-slim AS base-bookworm
FROM debian:trixie-slim AS base-trixie
FROM ubuntu:24.04 AS base-noble
FROM ubuntu:26.04 AS base-resolute
# Canonical's ubuntu:26.04 OCI image adds /usr/bin/pebble (the rocks service manager, Go, not dpkg-owned, so apt never
# patches it); nothing here uses it and its Go stdlib carries Critical/High CVEs (speechrec AI-612).
RUN rm -f /usr/bin/pebble && rm -rf /var/lib/pebble
FROM base-${DEBIAN_BASE} AS debian-base
ARG DEBIAN_BASE
ENV DEBIAN_FRONTEND=noninteractive
# The image must be the base it was asked for.
RUN . /etc/os-release && [ "$VERSION_CODENAME" = "$DEBIAN_BASE" ] || \
        { echo "ERROR: image base is '$VERSION_CODENAME', but DEBIAN_BASE=$DEBIAN_BASE" >&2; exit 1; }
RUN echo "APT::Get::Assume-Yes \"true\";\nAPT::Get::allow \"true\";" | tee -a  /etc/apt/apt.conf.d/90_no_prompt && \
    echo "APT::Keep-Downloaded-Packages \"false\";" | tee -a  /etc/apt/apt.conf.d/91_no_cache && \
    apt-get update
# Stage 1: Build OpenFST 1.7.2 from source
FROM debian-base as openfst-builder

ARG OPENFST_VERSION=1.7.2
ARG JOBS=4

# Install build dependencies for OpenFST
RUN apt-get update && \
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
    # OpenFst 1.7.2 on newer compilers, as rev-kaldi's tools/Makefile patches it. GCC 14+ (trixie): a raw pointer
    # can't be assigned to a unique_ptr. GCC 15+ (resolute) checks uninstantiated template bodies: VectorHashBiTable's
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
FROM debian-base

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
