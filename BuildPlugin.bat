@echo off
REM ============================================
REM Asymmetric Camera Plugin Build Manager
REM ============================================

setlocal enabledelayedexpansion

REM ============================================
REM Configuration
REM ============================================

REM Set your Unreal Engine 5.4 installation path here
REM If not set, script will try to auto-detect
set "UE5_PATH="

REM Project configuration
set "PROJECT_ROOT=%~dp0"
set "PROJECT_NAME=MyCustomCam"
set "PROJECT_FILE=%PROJECT_ROOT%%PROJECT_NAME%.uproject"
set "PLUGIN_NAME=AsymmetricCamera"
set "PLUGIN_PATH=%PROJECT_ROOT%Plugins\%PLUGIN_NAME%"

REM ============================================
REM Auto-detect UE5 Path
REM ============================================

if "%UE5_PATH%"=="" (
    echo Attempting to auto-detect Unreal Engine 5.4 installation...

    REM Common installation paths
    if exist "D:\Ue\UE\UE_5.4\Engine\Build\BatchFiles\Build.bat" (
        set "UE5_PATH=D:\Ue\UE\UE_5.4"
        echo Found UE5.4 at: !UE5_PATH!
    ) else if exist "C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" (
        set "UE5_PATH=C:\Program Files\Epic Games\UE_5.4"
        echo Found UE5.4 at: !UE5_PATH!
    ) else if exist "D:\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" (
        set "UE5_PATH=D:\Epic Games\UE_5.4"
        echo Found UE5.4 at: !UE5_PATH!
    ) else if exist "C:\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" (
        set "UE5_PATH=C:\Epic Games\UE_5.4"
        echo Found UE5.4 at: !UE5_PATH!
    ) else (
        echo ERROR: Could not auto-detect Unreal Engine 5.4 installation
        echo Please set UE5_PATH variable in this script
        pause
        exit /b 1
    )
)

REM Set tool paths
set "UBT=%UE5_PATH%\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe"
set "UAT=%UE5_PATH%\Engine\Build\BatchFiles\RunUAT.bat"
set "GENPROJ=%UE5_PATH%\Engine\Build\BatchFiles\GenerateProjectFiles.bat"

REM ============================================
REM Main Menu
REM ============================================

:menu
cls
echo ============================================
echo  Asymmetric Camera Plugin Build Manager
echo ============================================
echo.
echo Project: %PROJECT_NAME%
echo Plugin:  %PLUGIN_NAME%
echo UE5:     %UE5_PATH%
echo.
echo ============================================
echo  Actions:
echo ============================================
echo.
echo  1. Generate Project Files
echo  2. Build Plugin (Development Editor)
echo  3. Build Plugin (Shipping)
echo  4. Rebuild Plugin (Clean + Build)
echo  5. Clean Intermediate Files
echo  6. Package Plugin
echo  7. Open Project in Editor
echo  8. Open Visual Studio Solution
echo  9. Show Plugin Info
echo  0. Exit
echo.
echo ============================================

set /p choice="Enter your choice (0-9): "

if "%choice%"=="1" goto generate
if "%choice%"=="2" goto build_dev
if "%choice%"=="3" goto build_ship
if "%choice%"=="4" goto rebuild
if "%choice%"=="5" goto clean
if "%choice%"=="6" goto package
if "%choice%"=="7" goto open_editor
if "%choice%"=="8" goto open_vs
if "%choice%"=="9" goto info
if "%choice%"=="0" goto end

echo Invalid choice. Please try again.
timeout /t 2
goto menu

REM ============================================
REM Generate Project Files
REM ============================================

:generate
echo.
echo ============================================
echo Generating Project Files...
echo ============================================
echo.

call "%GENPROJ%" -project="%PROJECT_FILE%" -game -engine

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to generate project files
    pause
    goto menu
)

echo.
echo Project files generated successfully!
pause
goto menu

REM ============================================
REM Build Plugin - Development Editor
REM ============================================

:build_dev
echo.
echo ============================================
echo Building Plugin (Development Editor)...
echo ============================================
echo.

call "%UE5_PATH%\Engine\Build\BatchFiles\Build.bat" %PROJECT_NAME%Editor Win64 Development -Project="%PROJECT_FILE%" -WaitMutex

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed
    pause
    goto menu
)

echo.
echo Build completed successfully!
pause
goto menu

REM ============================================
REM Build Plugin - Shipping
REM ============================================

:build_ship
echo.
echo ============================================
echo Building Plugin (Shipping)...
echo ============================================
echo.

call "%UE5_PATH%\Engine\Build\BatchFiles\Build.bat" %PROJECT_NAME% Win64 Shipping -Project="%PROJECT_FILE%" -WaitMutex

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed
    pause
    goto menu
)

echo.
echo Build completed successfully!
pause
goto menu

REM ============================================
REM Rebuild Plugin
REM ============================================

:rebuild
echo.
echo ============================================
echo Rebuilding Plugin...
echo ============================================
echo.

echo Cleaning intermediate files...
call :clean_internal

echo.
echo Building plugin...
call "%UE5_PATH%\Engine\Build\BatchFiles\Build.bat" %PROJECT_NAME%Editor Win64 Development -Project="%PROJECT_FILE%" -WaitMutex

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Rebuild failed
    pause
    goto menu
)

echo.
echo Rebuild completed successfully!
pause
goto menu

REM ============================================
REM Clean Intermediate Files
REM ============================================

:clean
echo.
echo ============================================
echo Cleaning Intermediate Files...
echo ============================================
echo.

call :clean_internal

echo.
echo Clean completed!
pause
goto menu

:clean_internal
if exist "%PROJECT_ROOT%Binaries" (
    echo Deleting Binaries...
    rmdir /s /q "%PROJECT_ROOT%Binaries"
)

if exist "%PROJECT_ROOT%Intermediate" (
    echo Deleting Intermediate...
    rmdir /s /q "%PROJECT_ROOT%Intermediate"
)

if exist "%PLUGIN_PATH%\Binaries" (
    echo Deleting Plugin Binaries...
    rmdir /s /q "%PLUGIN_PATH%\Binaries"
)

if exist "%PLUGIN_PATH%\Intermediate" (
    echo Deleting Plugin Intermediate...
    rmdir /s /q "%PLUGIN_PATH%\Intermediate"
)

if exist "%PROJECT_ROOT%.vs" (
    echo Deleting .vs folder...
    rmdir /s /q "%PROJECT_ROOT%.vs"
)

if exist "%PROJECT_ROOT%%PROJECT_NAME%.sln" (
    echo Deleting solution file...
    del /q "%PROJECT_ROOT%%PROJECT_NAME%.sln"
)

echo Intermediate files cleaned!
exit /b 0

REM ============================================
REM Package Plugin
REM ============================================

:package
echo.
echo ============================================
echo Packaging Plugin...
echo ============================================
echo.

set "OUTPUT_PATH=%PROJECT_ROOT%PackagedPlugins\%PLUGIN_NAME%"

if not exist "%PROJECT_ROOT%PackagedPlugins" mkdir "%PROJECT_ROOT%PackagedPlugins"

call "%UAT%" BuildPlugin -Plugin="%PLUGIN_PATH%\%PLUGIN_NAME%.uplugin" -Package="%OUTPUT_PATH%" -CreateSubFolder

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Package failed
    pause
    goto menu
)

echo.
echo Plugin packaged successfully!
echo Output: %OUTPUT_PATH%
pause
goto menu

REM ============================================
REM Open Editor
REM ============================================

:open_editor
echo.
echo ============================================
echo Opening Unreal Editor...
echo ============================================
echo.

start "" "%UE5_PATH%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJECT_FILE%"

goto menu

REM ============================================
REM Open Visual Studio
REM ============================================

:open_vs
echo.
echo ============================================
echo Opening Visual Studio...
echo ============================================
echo.

if not exist "%PROJECT_ROOT%%PROJECT_NAME%.sln" (
    echo Solution file not found. Generating project files first...
    call :generate
)

start "" "%PROJECT_ROOT%%PROJECT_NAME%.sln"

goto menu

REM ============================================
REM Show Plugin Info
REM ============================================

:info
cls
echo ============================================
echo  Plugin Information
echo ============================================
echo.
echo Plugin Name:     %PLUGIN_NAME%
echo Plugin Path:     %PLUGIN_PATH%
echo Project:         %PROJECT_NAME%
echo Project Path:    %PROJECT_ROOT%
echo UE5 Path:        %UE5_PATH%
echo.
echo ============================================
echo  Plugin Files:
echo ============================================
echo.

if exist "%PLUGIN_PATH%\%PLUGIN_NAME%.uplugin" (
    echo [OK] Plugin descriptor found
) else (
    echo [ERROR] Plugin descriptor not found!
)

if exist "%PLUGIN_PATH%\Source\%PLUGIN_NAME%" (
    echo [OK] Runtime module found
) else (
    echo [ERROR] Runtime module not found!
)

if exist "%PLUGIN_PATH%\Source\%PLUGIN_NAME%Editor" (
    echo [OK] Editor module found
) else (
    echo [ERROR] Editor module not found!
)

if exist "%PLUGIN_PATH%\README.md" (
    echo [OK] Documentation found
) else (
    echo [WARNING] Documentation not found
)

echo.
echo ============================================
echo  Module Files:
echo ============================================
echo.

dir /b "%PLUGIN_PATH%\Source\%PLUGIN_NAME%\Public\*.h" 2>nul
dir /b "%PLUGIN_PATH%\Source\%PLUGIN_NAME%\Private\*.cpp" 2>nul

echo.
pause
goto menu

REM ============================================
REM Exit
REM ============================================

:end
echo.
echo Goodbye!
timeout /t 1
exit /b 0
