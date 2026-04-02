#!/bin/bash

BASHRC_PATH="${HOME}/.bashrc"
 [ ! -f "$BASHRC_PATH" ] && touch "$BASHRC_PATH"

# add new environment variables
{
    echo ""
    echo "# setup dependencies (added $(date))"
    echo "# setup boot repos"
    echo 'source ${USER_REPOS_PATH}/boot/setup_boot.sh'
    echo "# setup ffnvcodec"
    echo 'source ${BOOT_REPOS_PATH}/libs/ffnvcodec/13.0.19/setup_ffnvcodec.sh'
    echo "# setup ffmpeg"
    echo 'source ${BOOT_REPOS_PATH}/libs/ffmpeg/7.1.3/ubuntu24_nvgpu/setup_ffmpeg.sh'
} >> "$BASHRC_PATH"
