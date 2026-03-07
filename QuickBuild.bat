@echo off
REM ============================================
REM Quick Build Script for Asymmetric Camera Plugin
REM ============================================

setlocal enabledelayedexpansion

echo ============================================
echo  Quick Build - Asymmetric Camera Plugin
echo ============================================
echo.

REM ============================================
REM Auto-detect UE5 Path (5.4 / 5.5 / 5.6)
REM ============================================
set "UE5_PATH="

REM --- Try common installation directories for each supported version ---
for %%V in (5.6 5.5 5.4) do (
    if "!UE5_PATH!"=="" (
        for %%D in (
            "D:\Ue\UE\UE_%%V"
            "C:\Program Files\Epic Games\UE_%%V"
            "D:\Epic Games\UE_%%V"
            "C:\Epic Games\UE_%%V"
            "E:\Epic Games\UE_%%V"
        ) do (
            if "!UE5_PATH!"=="" (
                if exist "%%~D\Engine\Build\BatchFiles\Build.bat" (
                    set "UE5_PATH=%%~D"
                    echo Found UE %%V at: %%~D
                )
            )
        )
    )
)

if "!UE5_PATH!"=="" (
    echo.
    echo ERROR: Could not find Unreal Engine 5.4 / 5.5 / 5.6 in common locations.
    echo.
    echo Please open this script in a text editor and set UE5_PATH manually:
    echo   set "UE5_PATH=C:\Your\Path\To\UE_5.X"
    echo.
    echo Checked directories:
    echo   D:\Ue\UE\UE_5.X
    echo   C:\Program Files\Epic Games\UE_5.X
    echo   D:\Epic Games\UE_5.X
    echo   C:\Epic Games\UE_5.X
    echo   E:\Epic Games\UE_5.X
    echo.
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
