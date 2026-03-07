@echo off
REM Generate compile_commands.json for clangd IntelliSense
REM Run this script whenever you add/remove source files or change Build.cs

setlocal enabledelayedexpansion

REM ============================================
REM Auto-detect UE5 Path (5.4 / 5.5 / 5.6)
REM ============================================
set "UE5_PATH="

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
                if exist "%%~D\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" (
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
    echo Please open this script in a text editor and set UE5_PATH manually,
    echo then re-run. Example:
    echo   set "UE5_PATH=C:\Your\Path\To\UE_5.X"
    echo.
    pause
    exit /b 1
)

set UBT="!UE5_PATH!\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe"
set PROJECT="%~dp0MyCustomCam.uproject"
set ENGINE_DB="!UE5_PATH!\compile_commands.json"
set OUTPUT="%~dp0compile_commands.json"

echo Generating compile_commands.json using: !UE5_PATH!
%UBT% -mode=GenerateClangDatabase -project=%PROJECT% -game -engine MyCustomCamEditor Win64 Development

if exist %ENGINE_DB% (
    copy /Y %ENGINE_DB% %OUTPUT% >nul
    echo Done: compile_commands.json copied to project root.
    echo Restart VSCode or run "clangd: Restart language server" to apply.
) else (
    echo Error: UBT did not produce compile_commands.json
)
pause
