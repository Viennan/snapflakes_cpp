FROM nvidia/cuda:12.9.1-cudnn-devel-ubuntu24.04

ARG HOST_USER=wiennan-dev
ARG HOST_UID=1000
ARG HOST_GID=1000

USER root

RUN apt update && \
    apt install -y \
        sudo \
        ca-certificates \
        gnupg \
        lsb-release \
        software-properties-common \
        nano \
        cmake \
        ninja-build \
        build-essential \
        gcc-14 g++-14 gdb \
        python3 \
        python3-pip \
        python3-dev \
        git \
        wget \
        curl \
        unzip \
        zip

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 140 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 140

RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - \
    && echo "deb https://apt.llvm.org/noble/ llvm-toolchain-noble-21 main" >> /etc/apt/sources.list.d/llvm.list \
    && echo "deb-src https://apt.llvm.org/noble/ llvm-toolchain-noble-21 main" >> /etc/apt/sources.list.d/llvm.list

RUN apt-get update && apt-get install -y \
    clang-21 \
    clangd-21 \
    clang-tidy-21 \
    clang-format-21 \
    clang-tools-21 \
    lld-21 \
    lldb-21 \
    libc++-21-dev \
    libc++abi-21-dev

RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-21 100 \
    && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-21 100 \
    && update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-21 100 \
    && update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-21 100 \
    && update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-21 100 \
    && update-alternatives --install /usr/bin/lld lld /usr/bin/lld-21 100 \
    && update-alternatives --install /usr/bin/lldb lldb /usr/bin/lldb-21 100

RUN set -e; \
    # process group conflict
    if getent group $HOST_GID > /dev/null 2>&1; then \
        EXISTING_GROUP=$(getent group $HOST_GID | cut -d: -f1); \
        echo "Using existing group: $EXISTING_GROUP (GID: $HOST_GID)"; \
        GROUP_NAME="$EXISTING_GROUP"; \
    else \
        GROUP_NAME="$HOST_USER"; \
        groupadd -g $HOST_GID $GROUP_NAME; \
        echo "Created new group: ${GROUP_NAME} (GID: $HOST_GID)"; \
    fi; \
    \
    # process user conflict
    if getent passwd $HOST_UID > /dev/null 2>&1; then \
        EXISTING_USER=$(getent passwd $HOST_UID | cut -d: -f1); \
        echo "User with UID $HOST_UID already exists: $EXISTING_USER"; \
        echo "Modifying existing user to match host configuration..."; \
        if [ "$EXISTING_USER" = "$HOST_USER" ]; then \
            echo "Existing user name of UID ${HOST_UID} matches target user name ${HOST_USER}}."; \
            usermod -g $GROUP_NAME -s /bin/bash $HOST_USER; \
        else \
            echo "Changing username from $EXISTING_USER to $HOST_USER"; \
            usermod -l ${HOST_USER} ${EXISTING_USER}; \
            usermod -g $GROUP_NAME -d /home/$HOST_USER -m -s /bin/bash $HOST_USER; \
        fi; \
    else \
        echo "Created new user: $HOST_USER (UID: $HOST_UID)"; \
        useradd -m -u $HOST_UID -g $GROUP_NAME -s /bin/bash $HOST_USER; \
    fi; \
    \
    # sudo no password
    if ! grep -q "^$HOST_USER" /etc/sudoers 2>/dev/null; then \
        echo "$HOST_USER ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers; \
        echo "Added $HOST_USER to sudoers"; \
    else \
        echo "User $HOST_USER already in sudoers"; \
    fi

USER $HOST_USER

ENV HOME=/home/$HOST_USER
ENV USER=$HOST_USER
ENV UID=$HOST_UID
ENV GID=$HOST_GID

USER root
