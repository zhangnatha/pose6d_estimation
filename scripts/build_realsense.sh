#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
WORK_DIR="${REPO_DIR}"
INSTALL_DIR="${REPO_DIR}/3rdparty/realsense"
SDK_VERSION="v2.54.2"

echo "开始编译并安装 Intel RealSense SDK ${SDK_VERSION} 到 ${INSTALL_DIR}"

if [ "${SKIP_APT:-0}" != "1" ]; then
    echo "安装必要的依赖..."
    sudo apt-get update
    sudo apt-get install -y git libssl-dev libusb-1.0-0-dev libudev-dev pkg-config libgtk-3-dev libglfw3-dev libgl1-mesa-dev libglu1-mesa-dev
else
    echo "SKIP_APT=1，跳过系统依赖安装。"
fi

cd "${WORK_DIR}"
if [ ! -d "librealsense" ]; then
    git clone https://github.com/IntelRealSense/librealsense.git
fi
cd librealsense
git checkout "${SDK_VERSION}"

if [ "${SKIP_UDEV:-0}" != "1" ]; then
    echo "配置 USB 权限..."
    sudo rm -rf /etc/udev/rules.d/99-realsense-libusb.rules
    sudo cp ./config/99-realsense-libusb.rules /etc/udev/rules.d/
    sudo udevadm control --reload-rules
    sudo udevadm trigger || true
else
    echo "SKIP_UDEV=1，跳过 udev 规则配置。"
fi

mkdir -p build
cd build
cmake .. -DFORCE_RSUSB_BACKEND=true -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=true -DBUILD_TOOLS=true -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}/"
make -j"$(nproc)"
make install

if [ -f "${INSTALL_DIR}/bin/realsense-viewer" ]; then
    echo "安装成功！可运行以下命令测试："
    echo "${INSTALL_DIR}/bin/realsense-viewer"
else
    echo "安装失败，请检查日志！"
    exit 1
fi

cd "${WORK_DIR}"
rm -rf librealsense
echo "RealSense SDK ${SDK_VERSION} 已成功安装到 ${INSTALL_DIR}"
