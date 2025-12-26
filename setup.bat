@echo off
REM Setup script for Better Audio Mixer
REM Downloads OBS plugin build dependencies

echo ========================================
echo Better Audio Mixer - Setup
echo ========================================
echo.

REM Check if git is available
where git >nul 2>&1
if errorlevel 1 (
    echo ERROR: Git is not installed or not in PATH
    echo Please install Git and try again.
    pause
    exit /b 1
)

REM Clone obs-plugintemplate for the cmake files
if exist ".cmake-temp" rmdir /s /q ".cmake-temp"
echo Cloning OBS plugin template for cmake files...
git clone --depth 1 --filter=blob:none --sparse https://github.com/obsproject/obs-plugintemplate .cmake-temp
if errorlevel 1 goto git_error

cd .cmake-temp
git sparse-checkout set cmake
if errorlevel 1 goto git_error
cd ..

REM Copy cmake directory
if exist "cmake" rmdir /s /q "cmake"
xcopy /E /I /Y ".cmake-temp\cmake" "cmake"
if errorlevel 1 goto copy_error

REM Cleanup
rmdir /s /q ".cmake-temp"

echo.
echo ========================================
echo Setup Complete!
echo ========================================
echo.
echo The cmake build system has been downloaded.
echo.
echo Next steps:
echo   1. Run build.bat to build the plugin
echo   2. The build will download additional dependencies automatically
echo.
pause
exit /b 0

:git_error
echo.
echo ERROR: Git clone failed!
echo Please check your internet connection and try again.
pause
exit /b 1

:copy_error
echo.
echo ERROR: Failed to copy cmake files!
pause
exit /b 1
