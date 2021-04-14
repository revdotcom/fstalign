# Using kaldi image for pre-built OpenFST
FROM kaldiasr/kaldi:2020-09 as kaldi-base

FROM debian:9.8

COPY --from=kaldi-base /opt/kaldi/tools/openfst /opt/openfst
ENV OPENFST_ROOT /opt/openfst

ARG JOBS=4

RUN apt-get update && \
    apt-get -y install \
    cmake \
    g++

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
