@echo off
REM Package script for Better Audio Mixer
REM Creates a distributable folder structure

set VERSION=1.0.0
set BUILD_CONFIG=RelWithDebInfo
set PLUGIN_NAME=better-audio-mixer
set PACKAGE_NAME=%PLUGIN_NAME%-v%VERSION%

echo ========================================
echo Packaging Better Audio Mixer
echo Version: %VERSION%
echo ========================================
echo.

REM Check if build exists
set DLL_PATH=build_x64\%BUILD_CONFIG%\%PLUGIN_NAME%.dll
if not exist "%DLL_PATH%" (
    echo ERROR: Built DLL not found at %DLL_PATH%
    echo Please run build.bat first.
    pause
    exit /b 1
)

REM Clean previous package
if exist "%PACKAGE_NAME%" (
    echo Removing old package folder...
    rmdir /s /q "%PACKAGE_NAME%"
)

REM Create folder structure
echo Creating package folder structure...
mkdir "%PACKAGE_NAME%"
mkdir "%PACKAGE_NAME%\obs-plugins"
mkdir "%PACKAGE_NAME%\obs-plugins\64bit"
mkdir "%PACKAGE_NAME%\data"
mkdir "%PACKAGE_NAME%\data\obs-plugins"
mkdir "%PACKAGE_NAME%\data\obs-plugins\%PLUGIN_NAME%"
mkdir "%PACKAGE_NAME%\data\obs-plugins\%PLUGIN_NAME%\locale"

REM Copy plugin DLL
echo Copying plugin files...
copy "%DLL_PATH%" "%PACKAGE_NAME%\obs-plugins\64bit\"

REM Copy data files (locale)
echo Copying data files...
if exist "data\locale\en-US.ini" (
    copy "data\locale\en-US.ini" "%PACKAGE_NAME%\data\obs-plugins\%PLUGIN_NAME%\locale\"
)

REM Copy PDB if it exists (for debugging)
set PDB_PATH=build_x64\%BUILD_CONFIG%\%PLUGIN_NAME%.pdb
if exist "%PDB_PATH%" (
    echo Copying debug symbols...
    copy "%PDB_PATH%" "%PACKAGE_NAME%\obs-plugins\64bit\"
)

REM Create ZIP if 7z is available
where 7z >nul 2>&1
if not errorlevel 1 (
    echo Creating ZIP archive...
    if exist "%PACKAGE_NAME%.zip" del "%PACKAGE_NAME%.zip"
    7z a -tzip "%PACKAGE_NAME%.zip" "%PACKAGE_NAME%\*" >nul
    echo Created: %PACKAGE_NAME%.zip
)

echo.
echo ========================================
echo Packaging Complete!
echo ========================================
echo.
echo Package folder: %PACKAGE_NAME%\
echo.
echo Contents:
echo   %PACKAGE_NAME%\
echo   +-- data\
echo   ^|   +-- obs-plugins\
echo   ^|       +-- %PLUGIN_NAME%\
echo   ^|           +-- locale\
echo   ^|               +-- en-US.ini
echo   +-- obs-plugins\
echo       +-- 64bit\
echo           +-- %PLUGIN_NAME%.dll
echo.
echo Installation:
echo   Copy the contents of %PACKAGE_NAME%\ to your OBS folder:
echo   - Portable: Your OBS folder (where obs64.exe is)
echo   - Installed: C:\Program Files\obs-studio\
echo.

pause
exit /b 0
