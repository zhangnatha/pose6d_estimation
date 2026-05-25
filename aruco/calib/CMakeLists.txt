cmake_minimum_required(VERSION 3.10)
project(ArucoCalib)

# 标定程序依赖 OpenCV 标定、高 GUI 与图像编解码模块。
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(MODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
get_filename_component(ROOT_DIR "${MODULE_DIR}/.." ABSOLUTE)
set(THIRDPARTY_DIR "${ROOT_DIR}/3rdparty")

if(WIN32)
    find_package(OpenCV REQUIRED COMPONENTS core calib3d highgui imgcodecs imgproc)
else()
    # Linux 默认从仓库自带 OpenCV 配置中查找。
    set(OpenCV_DIR "${THIRDPARTY_DIR}/opencv/lib/cmake/opencv4")
    if(NOT EXISTS "${OpenCV_DIR}/OpenCVConfig.cmake")
        message(FATAL_ERROR "OpenCVConfig.cmake not found in ${OpenCV_DIR}")
    endif()
    find_package(OpenCV REQUIRED COMPONENTS core calib3d highgui imgcodecs imgproc PATHS ${OpenCV_DIR} NO_DEFAULT_PATH)
endif()

add_executable(arucoCalib main.cpp)

target_include_directories(arucoCalib PRIVATE ${OpenCV_INCLUDE_DIRS})

# 复用公共相机层，统一接入 RealSense 与 Orbbec。
target_link_libraries(arucoCalib PRIVATE
    ${OpenCV_LIBS}
    poseCamera
)

if(UNIX)
    # 将 OpenCV 与相机 SDK 的运行库路径写入目标，方便直接运行。
    set(CALIB_RPATHS
        "${THIRDPARTY_DIR}/opencv/lib"
        "${THIRDPARTY_DIR}/realsense/lib"
        "${THIRDPARTY_DIR}/orbbec/lib"
        "${THIRDPARTY_DIR}/orbbec/lib/extensions/depthengine"
        "${THIRDPARTY_DIR}/orbbec/lib/extensions/filters"
        "${THIRDPARTY_DIR}/orbbec/lib/extensions/firmwareupdater"
        "${THIRDPARTY_DIR}/orbbec/lib/extensions/frameprocessor"
    )
    set_target_properties(arucoCalib PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
    set_target_properties(arucoCalib PROPERTIES INSTALL_RPATH "${CALIB_RPATHS}")
    set_target_properties(arucoCalib PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/config.yaml" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/")

if(WIN32)
    file(GLOB OPENCV_DLLS "${THIRDPARTY_DIR}/opencv/x64/mingw/bin/*.dll")
    foreach(dll ${OPENCV_DLLS})
        add_custom_command(TARGET arucoCalib POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${dll}"
            $<TARGET_FILE_DIR:arucoCalib>
        )
    endforeach()

    if(EXISTS "${THIRDPARTY_DIR}/realsense/lib/x64/realsense2.dll")
        add_custom_command(TARGET arucoCalib POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${THIRDPARTY_DIR}/realsense/lib/x64/realsense2.dll"
            $<TARGET_FILE_DIR:arucoCalib>
        )
    endif()

    if(EXISTS "${THIRDPARTY_DIR}/orbbec/lib/OrbbecSDK.dll")
        add_custom_command(TARGET arucoCalib POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${THIRDPARTY_DIR}/orbbec/lib/OrbbecSDK.dll"
            $<TARGET_FILE_DIR:arucoCalib>
        )
    endif()
    if(IS_DIRECTORY "${THIRDPARTY_DIR}/orbbec/lib/extensions")
        add_custom_command(TARGET arucoCalib POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${THIRDPARTY_DIR}/orbbec/lib/extensions"
            $<TARGET_FILE_DIR:arucoCalib>/extensions
        )
    endif()

    get_filename_component(MINGW_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    foreach(rt_dll libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll libgomp-1.dll)
        if(EXISTS "${MINGW_BIN_DIR}/${rt_dll}")
            add_custom_command(TARGET arucoCalib POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MINGW_BIN_DIR}/${rt_dll}"
                $<TARGET_FILE_DIR:arucoCalib>
            )
        endif()
    endforeach()
endif()
