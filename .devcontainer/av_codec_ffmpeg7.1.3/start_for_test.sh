#!/bin/bash

docker run -it \
    --name compile_ffmpeg7.1.3 \
    --user=${USER} \
    --mount type=bind,source=${HOME}/.ssh,target=/home/${USER}/.ssh \
    --mount type=bind,source=${HOME}/.gitconfig,target=/home/${USER}/.gitconfig \
    --mount type=bind,source=${HOME}/workspace,target=/home/${USER}/workspace \
    --runtime=nvidia \
    --env NVIDIA_VISIBLE_DEVICES=all \
    --env NVIDIA_DRIVER_CAPABILITIES=all \
    ${USER}/snapflakes_cpp/av_codec_ffmpeg7.1.3:latest /bin/bash
