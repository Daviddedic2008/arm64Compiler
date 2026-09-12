@echo off
setlocal enabledelayedexpansion

:: 1. Define folder and file names
set "BUILD_DIR=testingBuild"
set "SOURCE_FILE=src/tester/testGen.c"
set "EXE_NAME=testGen.exe"

echo [1/3] Preparing build environment...

:: 2. Create the testingBuild directory if it doesn't already exist
if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
    echo Created folder: %BUILD_DIR%
)

echo [2/3] Compiling %SOURCE_FILE%...

:: 3. Compile the file and place the executable inside the folder
gcc "%SOURCE_FILE%" -o "%BUILD_DIR%\%EXE_NAME%"

:: 4. Check if compilation was successful
if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Compilation failed. Please check your C code.
    pause
    exit /b %ERRORLEVEL%
)

echo [3/3] Execution successful. Running program now...
echo --------------------------------------------------
echo.

:: 5. Navigate to the folder and execute the program
cd "%BUILD_DIR%"
"%EXE_NAME%"

echo.
echo --------------------------------------------------
echo Program finished execution.
pause
