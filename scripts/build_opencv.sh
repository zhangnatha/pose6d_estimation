#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
WORK_DIR="${REPO_DIR}"
INSTALL_DIR="${REPO_DIR}/3rdparty/opencv"
OPENCV_VERSION="4.5.5"

echo "开始编译并安装 OpenCV ${OPENCV_VERSION} 到 ${INSTALL_DIR}"

if [ "${SKIP_APT:-0}" != "1" ]; then
    echo "安装必要的依赖..."
    sudo apt-get update
    sudo apt-get install -y git build-essential libgtk-3-dev libavcodec-dev libavformat-dev libswscale-dev \
        libjpeg-dev libpng-dev libtiff-dev zlib1g-dev python3-dev python3-numpy
else
    echo "SKIP_APT=1，跳过系统依赖安装。"
fi

cd "${WORK_DIR}"
if [ ! -d "opencv-${OPENCV_VERSION}" ]; then
    wget "https://github.com/opencv/opencv/archive/${OPENCV_VERSION}.tar.gz" -O "opencv-${OPENCV_VERSION}.tar.gz"
    tar -xzf "opencv-${OPENCV_VERSION}.tar.gz"
fi

if [ ! -d "opencv_contrib-${OPENCV_VERSION}" ]; then
    wget "https://github.com/opencv/opencv_contrib/archive/${OPENCV_VERSION}.tar.gz" -O "opencv_contrib-${OPENCV_VERSION}.tar.gz"
    tar -xzf "opencv_contrib-${OPENCV_VERSION}.tar.gz"
fi

cd "opencv-${OPENCV_VERSION}"
mkdir -p build
cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DOPENCV_EXTRA_MODULES_PATH="${WORK_DIR}/opencv_contrib-${OPENCV_VERSION}/modules" \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTS=OFF \
    -DBUILD_PERF_TESTS=OFF \
    -DWITH_OPENGL=ON \
    -DWITH_V4L=OFF \
    -DWITH_LIBV4L=OFF \
    -DWITH_XVID=OFF \
    -DWITH_FFMPEG=ON \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}"

make -j"$(nproc)"
make install

if [ -f "${INSTALL_DIR}/lib/cmake/opencv4/OpenCVConfig.cmake" ] && [ -f "${INSTALL_DIR}/lib/libopencv_core.so" ]; then
    echo "安装成功！OpenCVConfig.cmake 位于 ${INSTALL_DIR}/lib/cmake/opencv4"
else
    echo "安装失败，请检查日志！"
    exit 1
fi

cd "${WORK_DIR}"
rm -rf "opencv_contrib-${OPENCV_VERSION}" "opencv-${OPENCV_VERSION}" \
       "opencv_contrib-${OPENCV_VERSION}.tar.gz" "opencv-${OPENCV_VERSION}.tar.gz"
echo "OpenCV ${OPENCV_VERSION} 已成功安装到 ${INSTALL_DIR}"
