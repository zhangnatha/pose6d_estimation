@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
for %%I in ("%SCRIPT_DIR%\..") do set "REPO_DIR=%%~fI"
set "SOURCE_DIR=%REPO_DIR%"
set "INSTALL_DIR=%REPO_DIR%\3rdparty\opencv"
set "OPENCV_VERSION=4.5.5"

echo ===================================================
echo Starting build of OpenCV %OPENCV_VERSION% on Windows
echo Target install directory: %INSTALL_DIR%
echo ===================================================

if exist "%INSTALL_DIR%" (
    echo OpenCV is already compiled and installed in: %INSTALL_DIR%
    echo To force a rebuild, please delete that folder.
    exit /b 0
)

where cmake >nul 2>nul
if !ERRORLEVEL! neq 0 (
    echo [ERROR] CMake not found in PATH. Please install CMake and add it to PATH.
    exit /b 1
)

set "GEN_ARGS="
where cl >nul 2>nul
if !ERRORLEVEL! == 0 (
    echo MSVC [cl.exe] detected. Will use default Visual Studio generator.
) else (
    where mingw32-make >nul 2>nul
    if !ERRORLEVEL! == 0 (
        echo MinGW [mingw32-make] detected. Using MinGW Makefiles.
        set "GEN_ARGS=-G "MinGW Makefiles""
    ) else (
        echo [ERROR] Neither MSVC [cl.exe] nor MinGW [mingw32-make] found in PATH.
        exit /b 1
    )
)

if not exist "%SOURCE_DIR%\opencv-%OPENCV_VERSION%" (
    curl -L "https://github.com/opencv/opencv/archive/%OPENCV_VERSION%.tar.gz" -o "%SOURCE_DIR%\opencv-%OPENCV_VERSION%.tar.gz"
    if !ERRORLEVEL! neq 0 exit /b 1
    tar -xzf "%SOURCE_DIR%\opencv-%OPENCV_VERSION%.tar.gz" -C "%SOURCE_DIR%"
)

if not exist "%SOURCE_DIR%\opencv_contrib-%OPENCV_VERSION%" (
    curl -L "https://github.com/opencv/opencv_contrib/archive/%OPENCV_VERSION%.tar.gz" -o "%SOURCE_DIR%\opencv_contrib-%OPENCV_VERSION%.tar.gz"
    if !ERRORLEVEL! neq 0 exit /b 1
    tar -xzf "%SOURCE_DIR%\opencv_contrib-%OPENCV_VERSION%.tar.gz" -C "%SOURCE_DIR%"
)

if not exist "%SOURCE_DIR%\opencv_build" mkdir "%SOURCE_DIR%\opencv_build"
cd /d "%SOURCE_DIR%\opencv_build"

cmake ..\opencv-%OPENCV_VERSION% !GEN_ARGS! ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_SHARED_LIBS=ON ^
    -DOPENCV_EXTRA_MODULES_PATH="%SOURCE_DIR%\opencv_contrib-%OPENCV_VERSION%\modules" ^
    -DBUILD_EXAMPLES=OFF ^
    -DBUILD_TESTS=OFF ^
    -DBUILD_PERF_TESTS=OFF ^
    -DBUILD_opencv_apps=OFF ^
    -DBUILD_opencv_java=OFF ^
    -DBUILD_opencv_python2=OFF ^
    -DBUILD_opencv_python3=OFF ^
    -DBUILD_opencv_js=OFF ^
    -DBUILD_opencv_objc=OFF ^
    -DBUILD_opencv_gapi=OFF ^
    -DWITH_OPENGL=ON ^
    -DWITH_FFMPEG=ON ^
    -DCMAKE_INSTALL_PREFIX="%INSTALL_DIR%"
if !ERRORLEVEL! neq 0 exit /b 1

cmake --build . --config Release --target install -j 4
if !ERRORLEVEL! neq 0 exit /b 1

cd /d "%SOURCE_DIR%"
rd /s /q "%SOURCE_DIR%\opencv_build"
rd /s /q "%SOURCE_DIR%\opencv-%OPENCV_VERSION%"
rd /s /q "%SOURCE_DIR%\opencv_contrib-%OPENCV_VERSION%"
del "%SOURCE_DIR%\opencv-%OPENCV_VERSION%.tar.gz"
del "%SOURCE_DIR%\opencv_contrib-%OPENCV_VERSION%.tar.gz"

echo OpenCV %OPENCV_VERSION% build and install completed successfully.
echo Install directory: %INSTALL_DIR%
