cmake_minimum_required(VERSION 3.10)
project(stagMarkerGenerator)

# 生成器在 Windows 走 GDI+，在 Linux 走 Cairo + FreeType。
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(MODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
get_filename_component(ROOT_DIR "${MODULE_DIR}/.." ABSOLUTE)
set(THIRDPARTY_DIR "${ROOT_DIR}/3rdparty")

message(STATUS "-------------------------------------------------")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "C++ Standard: 11")
message(STATUS "-------------------------------------------------")

if(WIN32)
    message(STATUS "Configuring for Windows (GDI+ Backend)...")
    add_executable(stagMarkerGen main_windows.cpp)
    target_link_libraries(stagMarkerGen gdiplus)

    if(MSVC)
        add_compile_options(/utf-8)
        target_compile_options(stagMarkerGen PRIVATE /O2)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
        target_compile_options(stagMarkerGen PRIVATE -O2)
    endif()
else()
    message(STATUS "Configuring for Linux (Cairo Backend)...")
    # Linux 依赖 Cairo 与 FreeType 完成矢量绘制和字体渲染。
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CAIRO REQUIRED cairo)
    find_package(Freetype REQUIRED)

    add_executable(stagMarkerGen main_linux.cpp)
    target_include_directories(stagMarkerGen PRIVATE
        ${CAIRO_INCLUDE_DIRS}
        ${FREETYPE_INCLUDE_DIRS}
    )
    target_link_libraries(stagMarkerGen
        ${CAIRO_LIBRARIES}
        ${FREETYPE_LIBRARIES}
        pthread
        dl
    )
    target_compile_options(stagMarkerGen PRIVATE -O3)
endif()

# 运行时依赖的编码表、字体和默认配置复制到构建目录。
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/codes" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/..")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/front" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/..")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/config.yaml" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/")

if(WIN32)
    file(GLOB OPENCV_DLLS "${THIRDPARTY_DIR}/opencv/x64/mingw/bin/*.dll")
    foreach(dll ${OPENCV_DLLS})
        add_custom_command(TARGET stagMarkerGen POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${dll}"
            $<TARGET_FILE_DIR:stagMarkerGen>
        )
    endforeach()

    get_filename_component(MINGW_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    foreach(rt_dll libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll libgomp-1.dll)
        if(EXISTS "${MINGW_BIN_DIR}/${rt_dll}")
            add_custom_command(TARGET stagMarkerGen POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MINGW_BIN_DIR}/${rt_dll}"
                $<TARGET_FILE_DIR:stagMarkerGen>
            )
        endif()
    endforeach()
endif()
