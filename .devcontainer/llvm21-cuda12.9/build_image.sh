#!/bin/bash

set -e

docker build \
  --build-arg HOST_USER=$(whoami)-dev \
  --build-arg HOST_UID=$(id -u) \
  --build-arg HOST_GID=$(id -g) \
  -f dev.dockerfile \
  -t ${USER}/snapflakes_cpp:llvm21-cuda12.9-dev .
