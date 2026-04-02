#!/bin/bash

source ${BOOT_REPOS_PATH}/containers/tools/utils.sh

if ! is_image_existed "${USER}/ubuntu24/cuda12.9/dev:latest"; then
    echo "Build ${USER}/ubuntu24/cuda12.9/dev:latest"
    bash ${BOOT_REPOS_PATH}/containers/ubuntu24/cuda12.9/dev/build.sh
fi

echo "Build ${USER}/snapflakes_cpp/av_codec_ffmpeg7.1.3:latest"
docker buildx build \
    --build-context boot_repos=${BOOT_REPOS_PATH} \
    --build-arg IMAGE_DOMAIN_USER=${USER} \
    --build-arg TARGET_USER=${USER} \
    -f build.dockerfile \
    -t ${USER}/snapflakes_cpp/av_codec_ffmpeg7.1.3:latest \
    .
