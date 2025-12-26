@echo off
REM Build script for Better Audio Mixer
REM Configures, builds, and packages the plugin

set VERSION=1.0.0
set BUILD_CONFIG=RelWithDebInfo

echo ========================================
echo Better Audio Mixer
echo Build Script
echo Version: %VERSION%
echo ========================================
echo.

REM Check if cmake folder exists
if not exist "cmake" (
    echo ERROR: cmake folder not found!
    echo Please run setup.bat first.
    pause
    exit /b 1
)

REM Step 1: Configure
echo [1/3] Configuring CMake...
echo.

cmake --preset windows-x64
if errorlevel 1 (
    echo.
    echo ========================================
    echo ERROR: CMake configuration failed!
    echo ========================================
    echo.
    echo Make sure you have:
    echo   1. Visual Studio 2022 installed
    echo   2. CMake 3.22+ installed
    echo   3. Run this from a Developer Command Prompt or have VS in PATH
    echo.
    pause
    exit /b 1
)

echo.
echo ========================================
echo Configuration successful!
echo ========================================
echo.

REM Step 2: Build
echo [2/3] Building plugin...
cmake --build --preset windows-x64 --config %BUILD_CONFIG%
if errorlevel 1 (
    echo ERROR: Build failed!
    pause
    exit /b 1
)
echo.

REM Step 3: Package
echo [3/3] Packaging...
call "%~dp0package.bat"
if errorlevel 1 (
    echo ERROR: Packaging failed!
    pause
    exit /b 1
)
