cmake_minimum_required(VERSION 3.10)
project(STagMarkerPose)

# 位姿模块整合 OpenCV、STag 静态库与相机 SDK。
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(MODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
get_filename_component(ROOT_DIR "${MODULE_DIR}/.." ABSOLUTE)
set(THIRDPARTY_DIR "${ROOT_DIR}/3rdparty")
set(REALSENSE_DIR "${THIRDPARTY_DIR}/realsense")
set(ORBBEC_DIR "${THIRDPARTY_DIR}/orbbec")

if(WIN32)
    find_package(OpenCV REQUIRED)
else()
    # Linux 优先从仓库内 OpenCV 路径查找，找不到再回退系统环境。
    set(OpenCV_DIR "${THIRDPARTY_DIR}/opencv/lib/cmake/opencv4")
    if(NOT EXISTS "${OpenCV_DIR}")
        message(WARNING "OpenCV path does not exist: ${OpenCV_DIR}. Trying system OpenCV...")
        find_package(OpenCV REQUIRED)
    else()
        find_package(OpenCV REQUIRED PATHS ${OpenCV_DIR})
    endif()
endif()
message(STATUS "OpenCV found version: ${OpenCV_VERSION}")

if(WIN32)
    set(RS_SEARCH_PATHS
        "${REALSENSE_DIR}"
        "${REALSENSE_DIR}/lib/cmake/realsense2"
        "C:/Program Files (x86)/Intel RealSense SDK 2.0"
        "C:/Program Files/Intel RealSense SDK 2.0"
        "D:/Program Files (x86)/RealSense SDK 2.0"
        "D:/Program Files (x86)/Intel RealSense SDK 2.0"
        "D:/Program Files/Intel RealSense SDK 2.0"
    )
    find_package(realsense2 QUIET PATHS ${RS_SEARCH_PATHS})
    if(realsense2_FOUND)
        set(RS_LIB realsense2::realsense2)
        message(STATUS "RealSense package found via CMake: ${realsense2_VERSION}")
    else()
        find_library(RS_LIB
            NAMES realsense2
            PATHS
                "${REALSENSE_DIR}/lib/x64"
                "${REALSENSE_DIR}/lib"
                "C:/Program Files (x86)/Intel RealSense SDK 2.0/lib/x64"
                "C:/Program Files (x86)/Intel RealSense SDK 2.0/lib/x86"
                "C:/Program Files/Intel RealSense SDK 2.0/lib/x64"
                "C:/Program Files/Intel RealSense SDK 2.0/lib"
                "D:/Program Files (x86)/RealSense SDK 2.0/lib/x64"
                "D:/Program Files (x86)/RealSense SDK 2.0/lib/x86"
                "D:/Program Files (x86)/RealSense SDK 2.0/lib"
                "D:/Program Files (x86)/Intel RealSense SDK 2.0/lib/x64"
                "D:/Program Files (x86)/Intel RealSense SDK 2.0/lib/x86"
                "D:/Program Files (x86)/Intel RealSense SDK 2.0/lib"
                "D:/Program Files/Intel RealSense SDK 2.0/lib/x64"
        )
        find_path(RS_INCLUDE_DIR
            NAMES librealsense2/rs.hpp
            PATHS
                "${REALSENSE_DIR}/include"
                "C:/Program Files (x86)/Intel RealSense SDK 2.0/include"
                "C:/Program Files/Intel RealSense SDK 2.0/include"
                "D:/Program Files (x86)/RealSense SDK 2.0/include"
                "D:/Program Files (x86)/Intel RealSense SDK 2.0/include"
                "D:/Program Files/Intel RealSense SDK 2.0/include"
        )
    endif()
else()
    # Linux 侧通过本地三方目录定位 RealSense CMake 配置。
    set(realsense2_DIR "${REALSENSE_DIR}/lib/cmake/realsense2")
    find_package(realsense2 REQUIRED PATHS ${realsense2_DIR} NO_DEFAULT_PATH)
    set(RS_LIB realsense2::realsense2)
    set(RS_INCLUDE_DIR "${REALSENSE_DIR}/include")
endif()

if(NOT RS_LIB)
    message(FATAL_ERROR "RealSense SDK not found. Set realsense2_DIR or install Intel RealSense SDK 2.0.")
endif()

if(WIN32)
    find_path(ORBBEC_INCLUDE_DIR
        NAMES libobsensor/ObSensor.hpp
        PATHS
            "$ENV{ORBBEC_SDK_ROOT}/include"
            "C:/Program Files/OrbbecSDK/include"
            "C:/Program Files (x86)/OrbbecSDK/include"
            "D:/Program Files/OrbbecSDK/include"
            "${ORBBEC_DIR}/include"
    )
    find_library(ORBBEC_LIB
        NAMES OrbbecSDK
        PATHS
            "$ENV{ORBBEC_SDK_ROOT}/lib"
            "$ENV{ORBBEC_SDK_ROOT}/lib/x64"
            "C:/Program Files/OrbbecSDK/lib"
            "C:/Program Files/OrbbecSDK/lib/x64"
            "C:/Program Files (x86)/OrbbecSDK/lib"
            "C:/Program Files (x86)/OrbbecSDK/lib/x64"
            "D:/Program Files/OrbbecSDK/lib"
            "D:/Program Files/OrbbecSDK/lib/x64"
            "${ORBBEC_DIR}/lib"
    )
else()
    # Linux 侧直接在 Orbbec SDK 目录检索头文件与共享库。
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

set(STAG_INCLUDE_DIR
    "${THIRDPARTY_DIR}/stag/src"
    "${THIRDPARTY_DIR}/stag/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/stag/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/stag/src"
)
set(STAG_LIB_DIR ${THIRDPARTY_DIR}/stag/lib)

if(WIN32)
    find_library(STAG_LIB
        NAMES libstag.a stag staglib staglibd.lib staglibd
        PATHS
            "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/stag/lib"
            "${THIRDPARTY_DIR}/stag/lib"
            ${STAG_LIB_DIR}
        NO_DEFAULT_PATH
    )
    if(NOT STAG_LIB)
        message(FATAL_ERROR
            "STag library not found! \n"
            "   Searched in: ${STAG_LIB_DIR} and ${THIRDPARTY_DIR}/stag/build/... \n"
            "   Please compile STag manually as per 'pose/README.md' step 0."
        )
    else()
        message(STATUS "STag library found: ${STAG_LIB}")
    endif()
else()
    # Linux 默认使用预编译好的 libstag.a 静态库。
    set(STAG_LIB "${STAG_LIB_DIR}/libstag.a")
    if(NOT EXISTS "${STAG_LIB}")
        message(FATAL_ERROR "STag static library not found at: ${STAG_LIB}")
    else()
        message(STATUS "STag static library found: ${STAG_LIB}")
    endif()
endif()

add_executable(stagMarkerPose main.cpp)

# 暴露 OpenCV、相机 SDK 与 STag 的头文件搜索路径。
target_include_directories(stagMarkerPose PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${RS_INCLUDE_DIR}
    ${ORBBEC_INCLUDE_DIR}
    ${STAG_INCLUDE_DIR}
)

target_link_libraries(stagMarkerPose PRIVATE
    ${STAG_LIB}
    ${RS_LIB}
    ${ORBBEC_LIB}
    ${OpenCV_LIBS}
    poseCamera
)

if(UNIX)
    target_link_libraries(stagMarkerPose PRIVATE pthread dl)
endif()

if(UNIX)
    # 写入依赖库路径，便于直接运行生成的可执行文件。
    set_target_properties(stagMarkerPose PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
    set(RUNTIME_LIB_PATHS
        "${THIRDPARTY_DIR}/realsense/lib"
        "${THIRDPARTY_DIR}/opencv/lib"
        "${THIRDPARTY_DIR}/stag/lib"
        "${THIRDPARTY_DIR}/orbbec/lib"
        "${THIRDPARTY_DIR}/orbbec/lib/extensions/depthengine"
        "${THIRDPARTY_DIR}/orbbec/lib/extensions/filters"
        "${THIRDPARTY_DIR}/orbbec/lib/extensions/firmwareupdater"
        "${THIRDPARTY_DIR}/orbbec/lib/extensions/frameprocessor"
    )
    set_target_properties(stagMarkerPose PROPERTIES INSTALL_RPATH "${RUNTIME_LIB_PATHS}")
    set_target_properties(stagMarkerPose PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/config.yaml" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assert/sample.png" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assert/intrinsic_calib.yaml" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/")

if(WIN32)
    file(GLOB OPENCV_DLLS "${THIRDPARTY_DIR}/opencv/x64/mingw/bin/*.dll")
    foreach(dll ${OPENCV_DLLS})
        add_custom_command(TARGET stagMarkerPose POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${dll}"
            $<TARGET_FILE_DIR:stagMarkerPose>
        )
    endforeach()

    if(EXISTS "${THIRDPARTY_DIR}/realsense/lib/x64/realsense2.dll")
        add_custom_command(TARGET stagMarkerPose POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${THIRDPARTY_DIR}/realsense/lib/x64/realsense2.dll"
            $<TARGET_FILE_DIR:stagMarkerPose>
        )
    endif()

    if(EXISTS "${THIRDPARTY_DIR}/orbbec/lib/OrbbecSDK.dll")
        add_custom_command(TARGET stagMarkerPose POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${THIRDPARTY_DIR}/orbbec/lib/OrbbecSDK.dll"
            $<TARGET_FILE_DIR:stagMarkerPose>
        )
    endif()
    if(IS_DIRECTORY "${THIRDPARTY_DIR}/orbbec/lib/extensions")
        add_custom_command(TARGET stagMarkerPose POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${THIRDPARTY_DIR}/orbbec/lib/extensions"
            $<TARGET_FILE_DIR:stagMarkerPose>/extensions
        )
    endif()

    get_filename_component(MINGW_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    foreach(rt_dll libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll libgomp-1.dll)
        if(EXISTS "${MINGW_BIN_DIR}/${rt_dll}")
            add_custom_command(TARGET stagMarkerPose POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MINGW_BIN_DIR}/${rt_dll}"
                $<TARGET_FILE_DIR:stagMarkerPose>
            )
        endif()
    endforeach()
endif()
