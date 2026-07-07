@echo off
setlocal enabledelayedexpansion

rem ============================================================
rem  Command-line build for the Spout2_DX12 plugin.
rem  Wraps RunUAT.bat BuildPlugin, the official way to compile
rem  a standalone UE plugin without opening the Editor.
rem
rem  Usage:
rem    Build.bat [EngineDir] [OutputDir]
rem
rem  Defaults:
rem    EngineDir = C:\Program Files\Epic Games\UE_5.8
rem    OutputDir = .\Build\Spout2_DX12
rem ============================================================

set "ENGINE_DIR=%~1"
if "%ENGINE_DIR%"=="" set "ENGINE_DIR=C:\Program Files\Epic Games\UE_5.8"

set "OUTPUT_DIR=%~2"
if "%OUTPUT_DIR%"=="" set "OUTPUT_DIR=%~dp0Build\Spout2_DX12"

set "PLUGIN_FILE=%~dp0Spout2_DX12.uplugin"
set "RUNUAT=%ENGINE_DIR%\Engine\Build\BatchFiles\RunUAT.bat"

if not exist "%RUNUAT%" (
    echo [ERROR] RunUAT.bat not found at "%RUNUAT%".
    echo         Pass the Engine install directory as the first argument, e.g.:
    echo         Build.bat "C:\Program Files\Epic Games\UE_5.8"
    exit /b 1
)

if not exist "%PLUGIN_FILE%" (
    echo [ERROR] Plugin descriptor not found at "%PLUGIN_FILE%".
    exit /b 1
)

echo Engine:  %ENGINE_DIR%
echo Plugin:  %PLUGIN_FILE%
echo Output:  %OUTPUT_DIR%
echo.

call "%RUNUAT%" BuildPlugin ^
    -Plugin="%PLUGIN_FILE%" ^
    -Package="%OUTPUT_DIR%" ^
    -TargetPlatforms=Win64 ^
    -Rocket

set "EXIT_CODE=%ERRORLEVEL%"
if not "%EXIT_CODE%"=="0" (
    echo.
    echo [FAILED] BuildPlugin exited with code %EXIT_CODE%.
    exit /b %EXIT_CODE%
)

echo.
echo [OK] Plugin built successfully. Output in "%OUTPUT_DIR%".
exit /b 0
