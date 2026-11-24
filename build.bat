@echo off
REM ==========================================================================
REM  Gospel Production Studio - build and launch
REM ==========================================================================
setlocal
cd /d "%~dp0"

echo.
echo  Gospel Production Studio - Build
echo  ================================
echo.

REM ---- Find CMake (on PATH, or inside Visual Studio) ------------------------
where cmake >nul 2>nul
if not errorlevel 1 goto :cmake_ok

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" goto :find_vs
echo  [X] Visual Studio not found. Install Visual Studio Community with
echo      the "Desktop development with C++" workload. See README.md.
goto :fail

:find_vs
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VSPATH=%%i"
set "PATH=%PATH%;%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
where cmake >nul 2>nul
if not errorlevel 1 goto :cmake_ok
echo  [X] CMake not found. In the Visual Studio Installer, tick
echo      "C++ CMake tools for Windows". See README.md.
goto :fail

:cmake_ok
echo  [OK] CMake found

REM ---- Configure, first time only -------------------------------------------
if exist "build\CMakeCache.txt" goto :build
echo  Configuring project - the first time takes a few minutes...
cmake -S . -B build -A x64
if errorlevel 1 goto :fail

:build
REM ---- Build -----------------------------------------------------------------
echo  Building Release version...
cmake --build build --config Release --parallel
if errorlevel 1 goto :fail

set "APP=build\GospelProductionStudio_artefacts\Release\Gospel Production Studio.exe"
if not exist "%APP%" (
    echo  [X] Build finished but the app was not found at:
    echo      %APP%
    goto :fail
)

echo.
echo  [OK] Build complete. Starting Gospel Production Studio...
start "" "%APP%"
exit /b 0

:fail
echo.
echo  Build did not finish. Copy the error lines above (or take a photo)
echo  and send them to Claude to fix.
echo.
pause
exit /b 1
