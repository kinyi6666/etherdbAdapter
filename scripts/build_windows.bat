@echo off
REM ============================================================================
REM etherAdapter — Windows build (Visual Studio 2022 / MSVC)
REM
REM   scripts\build_windows.bat [Debug|Release]   (default: Release)
REM
REM Output: build\bin\etherAdapter.exe (+ base.dll / net.dll),
REM         build\bin\csv2sqlite.exe, build\bin\device_sim.exe
REM ============================================================================
setlocal
cd /d "%~dp0.."

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: cmake not found in PATH. Install CMake or add it to PATH.
    exit /b 1
)

cmake -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1

cmake --build build --config %BUILD_TYPE% --parallel
if errorlevel 1 exit /b 1

echo.
echo Build OK (%BUILD_TYPE%). Binaries in build\bin:
dir /b build\bin
endlocal
