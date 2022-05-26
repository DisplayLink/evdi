# Copyright (c) 2022 DisplayLink (UK) Ltd.
FROM debian:buster
MAINTAINER Synaptics Technical Support <technical-enquiries@synaptics.com>

# Install basic tools
RUN apt-get update && apt-get install -y \
	coreutils \
	file \
	git \
	make \
	shellcheck \
	wget

RUN dpkg --add-architecture i386
RUN dpkg --add-architecture armhf
RUN dpkg --add-architecture arm64

# Install linux kernel build dependencies
RUN apt-get update && apt-get install -y bc bison flex libelf-dev libssl-dev

# Install libevdi dependencies
RUN apt-get update && apt-get install -y libdrm-dev libc6-dev

# C++ gcc and gcc crossompilers
RUN apt-get update && apt-get install -y gcc g++
RUN apt-get update \
 && apt-get install -y build-essential gcc-multilib g++-multilib
RUN apt-get update && apt-get install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
RUN apt-get update && apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

