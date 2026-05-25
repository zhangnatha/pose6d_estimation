@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
for %%I in ("%SCRIPT_DIR%\..") do set "REPO_DIR=%%~fI"
set "SOURCE_DIR=%REPO_DIR%"
set "TARGET_DIR=%REPO_DIR%\3rdparty\realsense"
set "URL=https://github.com/realsenseai/librealsense/releases/download/v2.58.1/RealSense.SDK-WIN10-2.58.1.10581.exe"
set "EXE_NAME=RealSense.SDK-WIN10-2.58.1.10581.exe"
:: Check if already installed
set "REAL_INSTALL_DIR="
for /f "tokens=*" %%i in ('powershell -NoProfile -Command "Get-ChildItem -Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall', 'HKLM:\SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall', 'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall' -ErrorAction SilentlyContinue | Get-ItemProperty | Where-Object DisplayName -like '*RealSense*' | Select-Object -ExpandProperty InstallLocation" 2^>nul') do (
    set "REAL_INSTALL_DIR=%%i"
)

if not defined REAL_INSTALL_DIR (
    echo RealSense SDK is not detected. Running installer...
    if not exist "%EXE_NAME%" (
        echo Downloading installer from %URL%...
        curl -L -o "%EXE_NAME%" "%URL%"
        if %errorlevel% neq 0 goto :error
    )
    echo Installing RealSense SDK...
    start /wait "" "%EXE_NAME%" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
    
    :: Query again after installation
    for /f "tokens=*" %%i in ('powershell -NoProfile -Command "Get-ChildItem -Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall', 'HKLM:\SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall', 'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall' -ErrorAction SilentlyContinue | Get-ItemProperty | Where-Object DisplayName -like '*RealSense*' | Select-Object -ExpandProperty InstallLocation" 2^>nul') do (
        set "REAL_INSTALL_DIR=%%i"
    )
)

if not defined REAL_INSTALL_DIR (
    echo ERROR: RealSense SDK installation directory could not be found.
    goto :error
)

:: Strip trailing backslash from REAL_INSTALL_DIR if present
if "%REAL_INSTALL_DIR:~-1%"=="\" set "REAL_INSTALL_DIR=%REAL_INSTALL_DIR:~0,-1%"

echo Found RealSense SDK install directory: %REAL_INSTALL_DIR%

if not exist "%TARGET_DIR%\include" mkdir "%TARGET_DIR%\include"
if not exist "%TARGET_DIR%\lib\x64" mkdir "%TARGET_DIR%\lib\x64"

xcopy "%REAL_INSTALL_DIR%\include" "%TARGET_DIR%\include" /E /I /Y
if exist "%REAL_INSTALL_DIR%\lib\x64" (
    xcopy "%REAL_INSTALL_DIR%\lib\x64\*.lib" "%TARGET_DIR%\lib\x64" /I /Y
) else (
    xcopy "%REAL_INSTALL_DIR%\lib\*.lib" "%TARGET_DIR%\lib\x64" /I /Y
)

if exist "%REAL_INSTALL_DIR%\bin\x64\realsense2.dll" (
    xcopy "%REAL_INSTALL_DIR%\bin\x64\realsense2.dll" "%TARGET_DIR%\lib\x64\" /Y
) else if exist "%REAL_INSTALL_DIR%\bin\realsense2.dll" (
    xcopy "%REAL_INSTALL_DIR%\bin\realsense2.dll" "%TARGET_DIR%\lib\x64\" /Y
)

if exist "%EXE_NAME%" del /f /q "%EXE_NAME%"
echo SUCCESS: RealSense SDK added to %TARGET_DIR%
exit /b 0

:error
echo FAILURE: Process failed.
exit /b 1
