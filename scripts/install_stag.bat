@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
for %%I in ("%SCRIPT_DIR%\..") do set "REPO_DIR=%%~fI"
set "SOURCE_DIR=%REPO_DIR%"
set "INSTALL_DIR=%REPO_DIR%\3rdparty\stag"
set "STAG_REPO=https://github.com/ManfredStoiber/stag.git"

echo ===================================================
echo Starting build of STag static library on Windows
echo Target install directory: %INSTALL_DIR%
echo ===================================================

where cmake >nul 2>nul
if !ERRORLEVEL! neq 0 exit /b 1

set "GEN_ARGS="
where cl >nul 2>nul
if !ERRORLEVEL! == 0 (
    echo MSVC [cl.exe] detected.
) else (
    where mingw32-make >nul 2>nul
    if !ERRORLEVEL! == 0 (
        set "GEN_ARGS=-G "MinGW Makefiles""
    ) else (
        exit /b 1
    )
)

if not exist "%SOURCE_DIR%\temp_stag" mkdir "%SOURCE_DIR%\temp_stag"
cd /d "%SOURCE_DIR%\temp_stag"
if not exist "stag" (
    git clone --depth 1 %STAG_REPO% stag
    if !ERRORLEVEL! neq 0 exit /b 1
)
cd stag
if not exist build mkdir build
cd build

set "OPENCV_DIR="
if exist "%SOURCE_DIR%\3rdparty\opencv" set "OPENCV_DIR=%SOURCE_DIR%\3rdparty\opencv"

if not "%OPENCV_DIR%"=="" (
    cmake .. !GEN_ARGS! -DCMAKE_BUILD_TYPE=Release -DOpenCV_DIR="!OPENCV_DIR!"
) else (
    cmake .. !GEN_ARGS! -DCMAKE_BUILD_TYPE=Release
)
if !ERRORLEVEL! neq 0 exit /b 1

cmake --build . --config Release
if !ERRORLEVEL! neq 0 exit /b 1

if not exist "%INSTALL_DIR%\lib" mkdir "%INSTALL_DIR%\lib"
if not exist "%INSTALL_DIR%\include" mkdir "%INSTALL_DIR%\include"

if exist "libstaglib.a" (
    copy /y "libstaglib.a" "%INSTALL_DIR%\lib\libstag.a"
) else if exist "Release\staglib.lib" (
    copy /y "Release\staglib.lib" "%INSTALL_DIR%\lib\stag.lib"
) else if exist "staglib.lib" (
    copy /y "staglib.lib" "%INSTALL_DIR%\lib\stag.lib"
)

if exist "..\src\Stag.h" copy /y "..\src\Stag.h" "%INSTALL_DIR%\include\Stag.h"

cd /d "%SOURCE_DIR%"
rd /s /q "%SOURCE_DIR%\temp_stag"
echo STag static library build and install completed successfully.
echo Install directory: %INSTALL_DIR%
