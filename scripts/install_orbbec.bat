@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
for %%I in ("%SCRIPT_DIR%\..") do set "REPO_DIR=%%~fI"
set "SOURCE_DIR=%REPO_DIR%"
set "TARGET_DIR=%REPO_DIR%\3rdparty\orbbec"
set "URL=https://github.com/orbbec/OrbbecSDK_v2/releases/download/v2.8.6/OrbbecSDK_v2.8.6_202604272251_6399409_win_x64.zip"
set "ZIP_NAME=OrbbecSDK_win_x64.zip"
set "TEMP_EXTRACT_DIR=%SOURCE_DIR%\OrbbecSDK_v2.8.6_202604272251_6399409_win_x64"

echo ========================================================
echo Process started: Extracting Orbbec SDK
echo Target Directory: %TARGET_DIR%
echo ========================================================

if not exist "%ZIP_NAME%" (
    curl -L -o "%ZIP_NAME%" "%URL%"
    if %errorlevel% neq 0 goto :error
)

if exist "%TEMP_EXTRACT_DIR%" rmdir /s /q "%TEMP_EXTRACT_DIR%"
mkdir "%TEMP_EXTRACT_DIR%"

powershell -Command "Expand-Archive -Path '%ZIP_NAME%' -DestinationPath '%TEMP_EXTRACT_DIR%' -Force"
if %errorlevel% neq 0 goto :error
if not exist "%TEMP_EXTRACT_DIR%\include" goto :error

if not exist "%TARGET_DIR%\include" mkdir "%TARGET_DIR%\include"
if not exist "%TARGET_DIR%\lib" mkdir "%TARGET_DIR%\lib"
if not exist "%TARGET_DIR%\bin" mkdir "%TARGET_DIR%\bin"

xcopy "%TEMP_EXTRACT_DIR%\include" "%TARGET_DIR%\include" /E /I /Y
xcopy "%TEMP_EXTRACT_DIR%\lib" "%TARGET_DIR%\lib" /E /I /Y

if exist "%TEMP_EXTRACT_DIR%\bin\OrbbecSDK.dll" (
    xcopy "%TEMP_EXTRACT_DIR%\bin\OrbbecSDK.dll" "%TARGET_DIR%\bin\" /Y
    xcopy "%TEMP_EXTRACT_DIR%\bin\OrbbecSDK.dll" "%TARGET_DIR%\lib\" /Y
)

if exist "%TEMP_EXTRACT_DIR%\bin\extensions" (
    if not exist "%TARGET_DIR%\lib\extensions" mkdir "%TARGET_DIR%\lib\extensions"
    xcopy "%TEMP_EXTRACT_DIR%\bin\extensions" "%TARGET_DIR%\lib\extensions" /E /I /Y
)

if exist "%ZIP_NAME%" del /f /q "%ZIP_NAME%"
if exist "%TEMP_EXTRACT_DIR%" rmdir /s /q "%TEMP_EXTRACT_DIR%"
echo SUCCESS: Orbbec SDK added to %TARGET_DIR%
exit /b 0

:error
echo FAILURE: Process failed.
if exist "%TEMP_EXTRACT_DIR%" rmdir /s /q "%TEMP_EXTRACT_DIR%"
exit /b 1
