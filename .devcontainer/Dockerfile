FROM ubuntu:22.04

# Avoid interactive prompts during package installs
ENV DEBIAN_FRONTEND=noninteractive

# ----------------------------------------------------------------
# Base build tools
# ----------------------------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    curl \
    wget \
    unzip \
    pkg-config \
    # Testing & coverage
    lcov \
    gcovr \
    # Analysis
    cppcheck \
    clang \
    clang-tools \
    # Python (for scripts and gcovr)
    python3 \
    python3-pip \
    # Useful utilities
    ca-certificates \
    file \
    less \
    nano \
    vim \
    && rm -rf /var/lib/apt/lists/*

# ----------------------------------------------------------------
# ARM cross-compiler (for Pico/embedded firmware target)
# ----------------------------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    libstdc++-arm-none-eabi-newlib \
    && rm -rf /var/lib/apt/lists/*

# ----------------------------------------------------------------
# Node.js 20 LTS + rp2040js (RP2040 emulator for boot smoke-test)
# ----------------------------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
    gnupg \
    && mkdir -p /etc/apt/keyrings \
    && curl -fsSL https://deb.nodesource.com/gpgkey/nodesource-repo.gpg.key \
       | gpg --dearmor -o /etc/apt/keyrings/nodesource.gpg \
    && echo "deb [signed-by=/etc/apt/keyrings/nodesource.gpg] \
       https://deb.nodesource.com/node_20.x nodistro main" \
       > /etc/apt/sources.list.d/nodesource.list \
    && apt-get update && apt-get install -y nodejs \
    && rm -rf /var/lib/apt/lists/* \
    && echo "Node.js ready — rp2040js is installed via docker/package.json at runtime"

# ----------------------------------------------------------------
# Create a non-root user that matches the UID/GID of the macOS host
# user (VS Code will override these via devcontainer.json remoteUser)
# ----------------------------------------------------------------
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=${USER_UID}

RUN groupadd --gid ${USER_GID} ${USERNAME} \
    && useradd --uid ${USER_UID} --gid ${USER_GID} -m -s /bin/bash ${USERNAME}

# Give the user passwordless sudo
RUN apt-get update && apt-get install -y sudo \
    && echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers \
    && rm -rf /var/lib/apt/lists/*

USER ${USERNAME}
WORKDIR /workspace
