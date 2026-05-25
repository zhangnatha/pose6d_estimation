cmake_minimum_required(VERSION 3.10)
project(ArucoMarkerGenerator)

# 生成器只依赖 ArUco 与基础图像模块，不需要相机 SDK。
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(MODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
get_filename_component(ROOT_DIR "${MODULE_DIR}/.." ABSOLUTE)
set(THIRDPARTY_DIR "${ROOT_DIR}/3rdparty")

if(WIN32)
    find_package(OpenCV REQUIRED COMPONENTS aruco imgcodecs imgproc core)
else()
    # Linux 默认优先使用仓库内 OpenCV，避免版本不一致。
    set(OpenCV_DIR "${THIRDPARTY_DIR}/opencv/lib/cmake/opencv4")
    if(NOT EXISTS "${OpenCV_DIR}/OpenCVConfig.cmake")
        message(FATAL_ERROR "OpenCVConfig.cmake not found in ${OpenCV_DIR}")
    endif()
    find_package(OpenCV REQUIRED COMPONENTS aruco imgcodecs imgproc core PATHS ${OpenCV_DIR} NO_DEFAULT_PATH)
endif()

add_executable(arucoMarkerGen main.cpp)

target_include_directories(arucoMarkerGen PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(arucoMarkerGen PRIVATE ${OpenCV_LIBS})

if(UNIX)
    # 写入 OpenCV 运行库路径，构建目录下可直接执行。
    set_target_properties(arucoMarkerGen PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
    set_target_properties(arucoMarkerGen PROPERTIES INSTALL_RPATH "${THIRDPARTY_DIR}/opencv/lib")
    set_target_properties(arucoMarkerGen PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/config.yaml" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/")

if(WIN32)
    file(GLOB OPENCV_DLLS "${THIRDPARTY_DIR}/opencv/x64/mingw/bin/*.dll")
    foreach(dll ${OPENCV_DLLS})
        add_custom_command(TARGET arucoMarkerGen POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${dll}"
            $<TARGET_FILE_DIR:arucoMarkerGen>
        )
    endforeach()

    get_filename_component(MINGW_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    foreach(rt_dll libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll libgomp-1.dll)
        if(EXISTS "${MINGW_BIN_DIR}/${rt_dll}")
            add_custom_command(TARGET arucoMarkerGen POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MINGW_BIN_DIR}/${rt_dll}"
                $<TARGET_FILE_DIR:arucoMarkerGen>
            )
        endif()
    endforeach()
endif()
