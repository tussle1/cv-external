@echo off
setlocal enabledelayedexpansion

echo [CatVapeExternal] Building...
echo.

where cmake >nul 2>nul
if errorlevel 1 (
    echo [!] cmake not found. Install CMake and Visual Studio Build Tools / VS.
    exit /b 1
)

if not exist build mkdir build
cd build

cmake .. -A x64
if errorlevel 1 (
    echo [!] CMake configure failed.
    exit /b 1
)

cmake --build . --config Release
if errorlevel 1 (
    echo [!] Build failed.
    exit /b 1
)

echo.
echo [ok] Built:
echo      build\bin\CatVapeExternal.exe
echo      build\bin\CatVapeBridge.dll
echo.
echo Put both files in the same folder, launch Roblox, then run CatVapeExternal.exe.
endlocal
