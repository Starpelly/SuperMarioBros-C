@echo off
setlocal

REM Define the build directory
set BUILD_DIR=build

REM Create the build directory if it doesn't exist
if not exist %BUILD_DIR% (
    mkdir %BUILD_DIR%
)

REM Change to the build directory
cd %BUILD_DIR%

REM Run CMake to generate Visual Studio 2022 solution files
cmake .. -G "Visual Studio 17 2022" -A x64

REM Build the solution
rem cmake --build . --config Release

echo.
echo Build files generated in %BUILD_DIR%\
pause