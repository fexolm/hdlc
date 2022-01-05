FROM ubuntu:20.04
ARG DEBIAN_FRONTEND=noninteractive
WORKDIR /tmp

RUN apt-get update
RUN apt-get install -y --no-install-recommends build-essential gnupg2 wget cmake ninja-build

# Install llvm
RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key --no-check-certificate | apt-key add -
RUN apt-get -y --no-install-recommends install llvm-12 llvm-12-dev zlib1g-dev
