# Build/test/lint image for modular-midi. No repo source is baked in here —
# the repo is bind-mounted at `docker run` time (see build-docker.ps1 and
# CLAUDE.md), so this image only needs to change when the toolchain itself
# changes, not on every code change.

FROM ubuntu:24.04

ARG ARM_TOOLCHAIN_VERSION=14.3.rel1
ARG ARM_TOOLCHAIN_URL=https://developer.arm.com/-/media/Files/downloads/gnu/${ARM_TOOLCHAIN_VERSION}/binrel/arm-gnu-toolchain-${ARM_TOOLCHAIN_VERSION}-x86_64-arm-none-eabi.tar.xz

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        clang-tidy \
        git \
        python3 \
        curl \
        ca-certificates \
        xz-utils \
    && rm -rf /var/lib/apt/lists/*

# ARM GNU Toolchain is pinned to the exact release used for the native
# Windows build (see setup.md / build.ps1), not the distro-packaged
# arm-none-eabi-gcc, which would drift to a different version.
RUN curl -fsSL "$ARM_TOOLCHAIN_URL" -o /tmp/arm-toolchain.tar.xz \
    && mkdir -p /opt/arm-gnu-toolchain \
    && tar -xf /tmp/arm-toolchain.tar.xz -C /opt/arm-gnu-toolchain --strip-components=1 \
    && rm /tmp/arm-toolchain.tar.xz

ENV PATH="/opt/arm-gnu-toolchain/bin:${PATH}"

# The repo is bind-mounted, usually owned by a different UID than this
# container's (root); git refuses to operate on such directories by default
# ("detected dubious ownership") unless told it's safe.
RUN git config --system --add safe.directory '*'

WORKDIR /workspace
