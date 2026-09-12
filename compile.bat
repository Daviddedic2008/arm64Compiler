@echo off
SETLOCAL EnableDelayedExpansion

for /F "tokens=1,2 delims=#" %%a in ('"prompt #$H#$E# & echo on & for %%b in (1) do rem"') do set ESC=%%b
set GREEN=!ESC![92m
set RED=!ESC![91m
set YELLOW=!ESC![93m
set BLUE=!ESC![96m
set RESET=!ESC![0m

set "inputFile=%~1"
set "testMode=%~2"

set /a NO_ERR=0
set /a EXPR_ERR=1
set /a DEL_ERR=2
set /a UNDEF_ERR=3
set /a OP_DEPTH_ERR=4
set /a BRANCH_DEPTH_ERR=5
set /a VAR_LIMIT_ERR=6

if exist "build\GPC.exe" (
    echo.
    echo %GREEN%[SUCCESS] Build found.%RESET%
    echo %YELLOW%--- Running ---%RESET%
    echo.
    build\GPC.exe %inputFile% %testMode%
) else (
    echo.
    echo %RED%[ERROR] Compiler binary not found. %RESET%
    pause
)