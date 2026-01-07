ARG DEBIAN_BASE=bullseye
# Using kaldi image for pre-built OpenFST, version is 1.7.2
FROM 414502572119.dkr.ecr.us-west-2.amazonaws.com/kaldi:v_5.5.985_4.0.8_${DEBIAN_BASE} AS kaldi-base
FROM debian:${DEBIAN_BASE}-slim AS debian-base
RUN echo "APT::Get::Assume-Yes \"true\";\nAPT::Get::allow \"true\";" | tee -a  /etc/apt/apt.conf.d/90_no_prompt && \
    echo "APT::Keep-Downloaded-Packages \"false\";" | tee -a  /etc/apt/apt.conf.d/91_no_cache && \
    apt-get update


FROM debian-base AS stage

COPY --from=kaldi-base /kaldi/tools/openfst /opt/openfst
ENV OPENFST_ROOT=/opt/openfst

ARG JOBS=4

RUN apt-get install \
    cmake \
    g++ \
    libicu-dev \
    ccache \
    git

RUN mkdir /fstalign
COPY CMakeLists.txt /fstalign/CMakeLists.txt
COPY src /fstalign/src
COPY test /fstalign/test
COPY third-party /fstalign/third-party
COPY sample_data /fstalign/sample_data

WORKDIR /fstalign
RUN git apply catch2.patch
RUN --mount=type=cache,target=/root/.ccache,sharing=locked  \
    mkdir -p /fstalign/build && \
    cd /fstalign/build && \
    rm -rf * && \
    cmake .. -DOPENFST_ROOT="${OPENFST_ROOT}" -DDYNAMIC_OPENFST=OFF && \
    make -j${JOBS} VERBOSE=1 && \
    mkdir -p /fstalign/bin && \
    cp /fstalign/build/fstalign /fstalign/bin && \
    strip /fstalign/bin/*
RUN make test
FROM debian-base AS release
WORKDIR /fstalign
RUN if [ "${DEBIAN_BASE}" = "bullseye" ]; then \
        apt-get install --no-install-recommends libicu67; \
    else \
        apt-get install --no-install-recommends libicu72; \
    fi; \
    rm -rf /var/lib/apt/lists/*
COPY tools /fstalign/tools
COPY --from=stage /fstalign/bin   /fstalign/bin
ENV PATH=/fstalign/bin:$PATH
