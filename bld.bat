@echo off
SETLOCAL EnableDelayedExpansion

for /F "tokens=1,2 delims=#" %%a in ('"prompt #$H#$E# & echo on & for %%b in (1) do rem"') do set ESC=%%b
set GREEN=!ESC![92m
set RED=!ESC![91m
set YELLOW=!ESC![93m
set BLUE=!ESC![96m
set RESET=!ESC![0m

set "inputFile=%~1"

:: bld -d = debug, bld -g = gdb debug build (build-debug, run with debug.bat), bld -r or bld = release
set "buildPreset=x64-release"
set "configPreset=x64-gnu-release"
set "buildDir=build"
if /I "%~1"=="-d" (
    set "buildPreset=x64-debug"
    set "configPreset=x64-gnu-debug"
    set "buildDir=build"
) else if /I "%~1"=="-g" (
    set "buildPreset=x64-gdb"
    set "configPreset=x64-gnu-gdb"
    set "buildDir=build-debug"
) else if not "%~1"=="" if /I not "%~1"=="-r" (
    echo %RED%[ERROR] Unknown option "%~1". Use -d for debug, -g for gdb debug, -r or nothing for release.%RESET%
    exit /b 1
)

if not exist "!buildDir!" (
    echo %BLUE%[INFO] Configuring !configPreset!...%RESET%
    cmake --preset !configPreset!
)

echo %YELLOW%--- Compiling (!buildPreset!) ---%RESET%
cmake --build --preset !buildPreset!

if %errorlevel% equ 0 (
    echo.
    echo %GREEN%[SUCCESS] Build finished successfully.%RESET%
) else (
    echo.
    echo %RED%[ERROR] Build failed. Check the errors above.%RESET%
    pause
)