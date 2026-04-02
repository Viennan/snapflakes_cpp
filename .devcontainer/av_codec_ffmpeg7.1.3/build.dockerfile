ARG IMAGE_DOMAIN_USER=wiennan

FROM ${IMAGE_DOMAIN_USER}/ubuntu24/cuda12.9/dev:latest

ARG TARGET_USER=wiennan

USER ${TARGET_USER}

RUN --mount=type=bind,from=boot_repos,target=/tmp/boot_repos \
    --mount=type=bind,target=/tmp/src_dir \
    bash /tmp/boot_repos/libs/ffmpeg/7.1.3/ubuntu24_nvgpu/install_build_deps.sh && \
    bash /tmp/boot_repos/libs/vulkan/1.3.296/apt_install.sh && \
    bash /tmp/src_dir/setup.sh

USER root
