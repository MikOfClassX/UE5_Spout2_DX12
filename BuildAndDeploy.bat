@echo off
setlocal

set "ENGINE_DIR=%~1"
if "%ENGINE_DIR%"=="" set "ENGINE_DIR=C:\Program Files\Epic Games\UE_5.8"

set "PACKAGE_DIR=%~2"
if "%PACKAGE_DIR%"=="" set "PACKAGE_DIR=%~dp0Build\Spout2_DX12"

if exist "%PACKAGE_DIR%" rmdir /s /q "%PACKAGE_DIR%"
if exist "%PACKAGE_DIR%" (
    echo [ERROR] Could not remove the previous package at "%PACKAGE_DIR%".
    exit /b 1
)

call "%~dp0Build.bat" "%ENGINE_DIR%" "%PACKAGE_DIR%"
if errorlevel 1 exit /b %ERRORLEVEL%

call "%~dp0Deploy.bat" "%ENGINE_DIR%" "%PACKAGE_DIR%"
exit /b %ERRORLEVEL%
