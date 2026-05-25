cmake_minimum_required(VERSION 3.10)
project(Aruco_STag_Pose)

# 统一使用 C++11，保证各子模块编译选项一致。
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 可按需关闭某一类工具，仅构建需要的模块。
option(BUILD_ARUCO_TOOLS "Build ArUco marker generator, calibration and pose tools" ON)
option(BUILD_STAG_TOOLS "Build STag marker generator and pose tools" ON)

# Windows 下优先使用仓库内第三方 OpenCV 路径，减少本机环境差异。
if(WIN32 AND NOT OpenCV_DIR)
    set(OpenCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/opencv" CACHE PATH "Path to OpenCV")
endif()

# 公共相机抽象层会被 ArUco 和 STag 位姿模块共同复用。
add_subdirectory(common/camera)

if(BUILD_ARUCO_TOOLS)
    add_subdirectory(aruco)
endif()

if(BUILD_STAG_TOOLS)
    add_subdirectory(stag)
endif()
