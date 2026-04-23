FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    clang \
    cmake \
    git \
    lld \
    ninja-build \
    pkg-config \
  && rm -rf /var/lib/apt/lists/*

ENV VST3_SDK_ROOT=/opt/vst3sdk
ENV LLVM_MINGW_ROOT=/opt/llvm-mingw
WORKDIR /work/YuckBeat
