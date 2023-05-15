# Using kaldi image for pre-built OpenFST, version is 1.6.7
FROM kaldiasr/kaldi:2020-09 as kaldi-base

FROM debian:9.8

COPY --from=kaldi-base /opt/kaldi/tools/openfst /opt/openfst
ENV OPENFST_ROOT /opt/openfst

ARG JOBS=4

#Update stretch repositories
RUN sed -i s/deb.debian.org/archive.debian.org/g /etc/apt/sources.list
RUN sed -i 's|security.debian.org|archive.debian.org/|g' /etc/apt/sources.list
RUN sed -i '/stretch-updates/d' /etc/apt/sources.list

RUN apt-get update && \
    apt-get -y install \
    cmake \
    g++ \
    libicu-dev

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
    cmake .. -DOPENFST_ROOT="${OPENFST_ROOT}" -DDYNAMIC_OPENFST=ON && \
    make -j${JOBS} VERBOSE=1 && \
    mkdir -p /fstalign/bin && \
    cp /fstalign/build/fstalign /fstalign/bin && \
    strip /fstalign/bin/*

COPY tools /fstalign/tools

ENV PATH \
    /fstalign/bin/:\
    $PATH
