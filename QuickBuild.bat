@echo off
REM ============================================
REM Quick Build Script for Asymmetric Camera Plugin
REM ============================================

setlocal enabledelayedexpansion

echo ============================================
echo  Quick Build - Asymmetric Camera Plugin
echo ============================================
echo.

REM Auto-detect UE5 Path
set "UE5_PATH="

if exist "D:\Ue\UE\UE_5.4\Engine\Build\BatchFiles\Build.bat" (
    set "UE5_PATH=D:\Ue\UE\UE_5.4"
) else if exist "C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" (
    set "UE5_PATH=C:\Program Files\Epic Games\UE_5.4"
) else if exist "D:\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" (
    set "UE5_PATH=D:\Epic Games\UE_5.4"
) else if exist "C:\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" (
    set "UE5_PATH=C:\Epic Games\UE_5.4"
) else (
    echo ERROR: Could not find Unreal Engine 5.4
    echo Please edit this script and set UE5_PATH manually
    pause
    exit /b 1
)

echo Using UE5 from: %UE5_PATH%
echo.

REM Project settings
set "PROJECT_ROOT=%~dp0"
set "PROJECT_NAME=MyCustomCam"
set "PROJECT_FILE=%PROJECT_ROOT%%PROJECT_NAME%.uproject"

echo Building %PROJECT_NAME%Editor (Development)...
echo.

call "%UE5_PATH%\Engine\Build\BatchFiles\Build.bat" %PROJECT_NAME%Editor Win64 Development -Project="%PROJECT_FILE%" -WaitMutex

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ============================================
    echo  BUILD FAILED
    echo ============================================
    pause
    exit /b 1
)

echo.
echo ============================================
echo  BUILD SUCCESSFUL!
echo ============================================
echo.
echo Plugin compiled successfully.
echo You can now open the project in Unreal Editor.
echo.
pause
