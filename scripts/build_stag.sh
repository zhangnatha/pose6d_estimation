#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
INSTALL_DIR="${REPO_DIR}/3rdparty/stag"
STAG_REPO="https://github.com/ManfredStoiber/stag.git"

echo "开始编译并安装 STag 标记库到 ${INSTALL_DIR}"

mkdir -p "${REPO_DIR}/temp_stag"
cd "${REPO_DIR}/temp_stag"
if [ ! -d "stag" ]; then
    git clone "${STAG_REPO}" stag
fi
cd stag

mkdir -p build
cd build

OPENCV_CMAKE_DIR="${REPO_DIR}/3rdparty/opencv/lib/cmake/opencv4"
if [ -d "${OPENCV_CMAKE_DIR}" ]; then
    cmake .. -DCMAKE_BUILD_TYPE=Release -DOpenCV_DIR="${OPENCV_CMAKE_DIR}"
else
    cmake .. -DCMAKE_BUILD_TYPE=Release
fi

make -j"$(nproc)"

mkdir -p "${INSTALL_DIR}/lib" "${INSTALL_DIR}/include"
if [ -f "libstaglib.a" ]; then
    cp libstaglib.a "${INSTALL_DIR}/lib/libstag.a"
fi
if [ -f "../src/Stag.h" ]; then
    cp ../src/Stag.h "${INSTALL_DIR}/include/Stag.h"
fi

cd "${REPO_DIR}"
rm -rf temp_stag
echo "STag 库已成功安装到 ${INSTALL_DIR}"
