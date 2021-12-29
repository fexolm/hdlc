FROM ubuntu:20.04
ARG DEBIAN_FRONTEND=noninteractive
WORKDIR /tmp

RUN apt-get update
RUN apt-get install -y --no-install-recommends build-essential wget tar cmake ninja-build gdb git clang-format

RUN wget --no-check-certificate https://github.com/llvm/llvm-project/releases/download/llvmorg-13.0.0/clang+llvm-13.0.0-x86_64-linux-gnu-ubuntu-20.04.tar.xz
RUN tar -xvf clang+llvm-13.0.0-x86_64-linux-gnu-ubuntu-20.04.tar.xz
RUN mv clang+llvm-13.0.0-x86_64-linux-gnu-ubuntu-20.04 llvmdir