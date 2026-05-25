cmake_minimum_required(VERSION 3.10)
project(PoseCamera)

# 公共相机库统一使用 C++11，供多个上层可执行程序复用。
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(THIRDPARTY_DIR "${ROOT_DIR}/3rdparty")
set(REALSENSE_DIR "${THIRDPARTY_DIR}/realsense")
set(ORBBEC_DIR "${THIRDPARTY_DIR}/orbbec")

if(WIN32)
    find_package(OpenCV REQUIRED COMPONENTS core imgcodecs imgproc)
else()
    # Linux 默认从仓库内 3rdparty/opencv 查找，避免误链系统版本。
    set(OpenCV_DIR "${THIRDPARTY_DIR}/opencv/lib/cmake/opencv4")
    if(NOT EXISTS "${OpenCV_DIR}/OpenCVConfig.cmake")
        message(FATAL_ERROR "OpenCVConfig.cmake not found in ${OpenCV_DIR}")
    endif()
    find_package(OpenCV REQUIRED COMPONENTS core imgcodecs imgproc PATHS ${OpenCV_DIR} NO_DEFAULT_PATH)
endif()

if(WIN32)
    set(RS_SEARCH_PATHS
        "${REALSENSE_DIR}"
        "${REALSENSE_DIR}/lib/cmake/realsense2"
    )
    find_package(realsense2 QUIET PATHS ${RS_SEARCH_PATHS})
    if(realsense2_FOUND)
        set(RS_LIB realsense2::realsense2)
    else()
        find_library(RS_LIB NAMES realsense2 PATHS
            "${REALSENSE_DIR}/lib/x64"
            "${REALSENSE_DIR}/lib"
        )
        find_path(RS_INCLUDE_DIR NAMES librealsense2/rs.hpp PATHS
            "${REALSENSE_DIR}/include"
        )
    endif()
else()
    # Linux 侧强制从本地三方目录加载 RealSense CMake 包。
    set(realsense2_DIR "${REALSENSE_DIR}/lib/cmake/realsense2")
    find_package(realsense2 REQUIRED PATHS ${realsense2_DIR} NO_DEFAULT_PATH)
    set(RS_LIB realsense2::realsense2)
    set(RS_INCLUDE_DIR "${REALSENSE_DIR}/include")
endif()

if(NOT RS_LIB)
    message(FATAL_ERROR "RealSense SDK not found. Set realsense2_DIR or install Intel RealSense SDK 2.0.")
endif()

if(WIN32)
    find_path(ORBBEC_INCLUDE_DIR NAMES libobsensor/ObSensor.hpp PATHS
        "$ENV{ORBBEC_SDK_ROOT}/include"
        "${ORBBEC_DIR}/include"
    )
    find_library(ORBBEC_LIB NAMES OrbbecSDK PATHS
        "$ENV{ORBBEC_SDK_ROOT}/lib"
        "$ENV{ORBBEC_SDK_ROOT}/lib/x64"
        "${ORBBEC_DIR}/lib"
    )
else()
    # Linux 侧直接从 Orbbec SDK 目录搜索共享库。
    set(ORBBEC_INCLUDE_DIR "${ORBBEC_DIR}/include")
    find_library(ORBBEC_LIB
        NAMES OrbbecSDK libOrbbecSDK.so
        PATHS "${ORBBEC_DIR}/lib"
        NO_DEFAULT_PATH
    )
endif()

if(NOT ORBBEC_INCLUDE_DIR OR NOT ORBBEC_LIB)
    message(FATAL_ERROR "Orbbec SDK not found. Set ORBBEC_SDK_ROOT or install/copy SDK to ${ORBBEC_DIR}.")
endif()

add_library(poseCamera STATIC
    CameraBase.cpp
    RealSenseCamera.cpp
    OrbbecCamera.cpp
)

# 对外暴露相机抽象头文件以及底层 SDK 头文件路径。
target_include_directories(poseCamera PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OpenCV_INCLUDE_DIRS}
    ${RS_INCLUDE_DIR}
    ${ORBBEC_INCLUDE_DIR}
)

target_link_libraries(poseCamera PUBLIC
    ${OpenCV_LIBS}
    ${RS_LIB}
    ${ORBBEC_LIB}
)

if(UNIX)
    # 运行时写入 RPATH，减少手动配置 LD_LIBRARY_PATH 的需求。
    target_link_libraries(poseCamera PUBLIC pthread dl)
    set(ORBBEC_RPATHS
        "${ORBBEC_DIR}/lib"
        "${ORBBEC_DIR}/lib/extensions/depthengine"
        "${ORBBEC_DIR}/lib/extensions/filters"
        "${ORBBEC_DIR}/lib/extensions/firmwareupdater"
        "${ORBBEC_DIR}/lib/extensions/frameprocessor"
    )
    set(REALSENSE_RPATHS "${REALSENSE_DIR}/lib")
    list(JOIN ORBBEC_RPATHS ";" ALL_CAMERA_RPATHS)
    list(APPEND ALL_CAMERA_RPATHS ${REALSENSE_RPATHS})
    set_target_properties(poseCamera PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
    set_target_properties(poseCamera PROPERTIES INSTALL_RPATH "${ALL_CAMERA_RPATHS}")
    set_target_properties(poseCamera PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()
