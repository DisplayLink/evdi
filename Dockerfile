# Copyright (c) 2022 DisplayLink (UK) Ltd.
FROM debian:trixie-slim
MAINTAINER Synaptics Technical Support <technical-enquiries@synaptics.com>

# Install basic tools
RUN apt-get update && apt-get install -y \
	curl \
	coreutils \
	fakeroot \
	file \
	git \
	lintian \
	make \
	shellcheck \
	pkg-config \
	qemu-system-x86 \
	wget && apt-get clean

# Install java for Synopsys detect
RUN apt-get update && apt-get install -y default-jre default-jdk && apt-get clean

ENV JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64/

RUN dpkg --add-architecture i386
RUN dpkg --add-architecture armhf
RUN dpkg --add-architecture arm64

# Install linux kernel build dependencies
RUN apt-get update && apt-get install -y bc bison flex libelf-dev libssl-dev && apt-get clean


# Install libevdi dependencies
RUN apt-get update && apt-get install -y libdrm-dev libc6-dev && apt-get clean


# C++ gcc and gcc crossompilers
RUN apt-get update && apt-get install -y gcc g++ && apt-get clean
RUN apt-get update \
 && apt-get install -y build-essential gcc-multilib g++-multilib && apt-get clean
RUN apt-get update && apt-get install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf && apt-get clean
RUN apt-get update && apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu && apt-get clean


# Dependencies for pybind
RUN apt-get update && apt-get install -y python3 libpython3-dev python3-pip python3-venv && apt-get clean
ENV VIRTUAL_ENV=/tmp/venv
# Set the virtual environment
RUN python3 -m venv $VIRTUAL_ENV
ENV PATH="$VIRTUAL_ENV/bin:$PATH"
RUN pip install pybind11


# needed to generate the compile_commands.json
RUN apt-get update && apt-get install -y bear && apt-get clean

# Install SonarQube utils
RUN cd /tmp && curl -ksSLo sonar-scanner-cli.zip https://artifactory.cam.displaylink.com:443/artifactory/third-party-tools/SonarQube/sonar-scanner-cli-linux.zip
RUN unzip -o /tmp/sonar-scanner-cli.zip -d /tmp && cp -r /tmp/sonar-scanner-*-linux/* /usr/local/ && chmod a+x /usr/local/bin/sonar-scanner

# Fetch linux source code, make the repo usable by multiple users and mark it safe for Git
RUN mkdir -p /usr/src && cd /usr/src && git clone -b master https://github.com/torvalds/linux.git && \
  cd /usr/src/linux && git config core.sharedRepository all && git fetch --tags && \
  chown -R root:root /usr/src/linux && \
  chmod -R a+rwX /usr/src/linux && \
  find /usr/src/linux -type d -exec chmod g+s {} + && \
  git -C /usr/src/linux config core.sharedRepository all && \
  git config --system --add safe.directory /usr/src/linux

