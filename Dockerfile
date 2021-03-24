FROM kaldiasr/kaldi:2020-09
ENV KALDI_ROOT /opt/kaldi

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
    cmake .. -DFST_KALDI_ROOT="${KALDI_ROOT}" -DDYNAMIC_KALDI=ON && \
    make -j${JOBS} VERBOSE=1 && \
    mkdir -p /fstalign/bin && \
    cp /fstalign/build/fstalign /fstalign/bin && \
    strip /fstalign/bin/*
