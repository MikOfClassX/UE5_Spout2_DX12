@echo off
setlocal

set "ENGINE_DIR=%~1"
if "%ENGINE_DIR%"=="" set "ENGINE_DIR=C:\Program Files\Epic Games\UE_5.8"

set "PACKAGE_DIR=%~2"
if "%PACKAGE_DIR%"=="" set "PACKAGE_DIR=%~dp0Build\Spout2_DX12"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Deploy.ps1" -EngineDir "%ENGINE_DIR%" -PackageDir "%PACKAGE_DIR%"
set "EXIT_CODE=%ERRORLEVEL%"

if not "%EXIT_CODE%"=="0" (
    echo [FAILED] Spout2_DX12 deployment exited with code %EXIT_CODE%.
    exit /b %EXIT_CODE%
)

echo [OK] Spout2_DX12 package deployed successfully.
exit /b 0
