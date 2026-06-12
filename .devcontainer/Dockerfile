FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG QT_VERSION=6.8.3

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    git \
    ninja-build \
    python3 \
    python3-pip \
    python3-venv \
    libgl1-mesa-dev \
    libxkbcommon-x11-0 \
    libxcb-cursor0 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-keysyms1 \
    libxcb-render-util0 \
    libxcb-xinerama0 \
    libxcb-xinput0 \
    libgtest-dev \
    sudo \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/aqt && \
    /opt/aqt/bin/pip install --no-cache-dir aqtinstall && \
    /opt/aqt/bin/aqt install-qt linux desktop ${QT_VERSION} gcc_64 -O /opt/Qt

RUN useradd -m vscode && echo "vscode ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/vscode

ENV CMAKE_PREFIX_PATH=/opt/Qt/${QT_VERSION}/gcc_64
USER vscode
WORKDIR /workspaces/rfp
