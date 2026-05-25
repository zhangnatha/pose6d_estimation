#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
INSTALL_DIR="${REPO_DIR}/3rdparty/orbbec"
SDK_DIR="${REPO_DIR}/OrbbecSDK_v2"
BUILD_DIR="${SDK_DIR}/build"

if [ "${SKIP_APT:-0}" != "1" ]; then
    echo "Installing dependencies..."
    sudo apt-get update
    sudo apt-get install -y build-essential
else
    echo "SKIP_APT=1，跳过系统依赖安装。"
fi

cd "${REPO_DIR}"
if [ ! -d "${SDK_DIR}" ]; then
    echo "Cloning OrbbecSDK_v2 repository..."
    git clone https://github.com/orbbec/OrbbecSDK_v2.git
else
    echo "OrbbecSDK_v2 directory already exists, pulling latest changes..."
    cd "${SDK_DIR}"
    git pull
    cd "${REPO_DIR}"
fi

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"
cmake .. -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}"
cmake --build . --config Release
make install

if [ "${SKIP_UDEV:-0}" != "1" ]; then
    cd "${INSTALL_DIR}/shared"
    sudo chmod +x ./install_udev_rules.sh
    sudo ./install_udev_rules.sh
    sudo udevadm control --reload
    sudo udevadm trigger
else
    echo "SKIP_UDEV=1，跳过 udev 规则配置。"
fi

if [ -d "${INSTALL_DIR}/lib" ] && [ -d "${INSTALL_DIR}/include" ]; then
    echo "Orbbec SDK installed successfully at ${INSTALL_DIR}"
else
    echo "Installation failed: lib or include directory not found"
    exit 1
fi

rm -rf "${SDK_DIR}"
