@echo off
REM ==========================================================================
REM  Gospel Production Studio - Stage 0 / Step 1
REM  Downloads Tracktion Engine (with its matching JUCE) into libs\
REM ==========================================================================
setlocal
cd /d "%~dp0"

echo.
echo  Gospel Production Studio - Setup
echo  ================================
echo.

where git >nul 2>nul
if errorlevel 1 (
    echo  [X] Git is not installed.
    echo      Install "Git for Windows" from https://git-scm.com and run this again.
    goto :fail
)
echo  [OK] Git found

if not exist libs mkdir libs

if exist "libs\tracktion_engine\modules\juce\CMakeLists.txt" (
    echo  [OK] Tracktion Engine and JUCE already downloaded
    goto :done
)

if exist "libs\tracktion_engine" (
    echo  Finishing an earlier, incomplete download...
    git -C libs\tracktion_engine submodule update --init --recursive --depth 1
    if errorlevel 1 goto :fail
    goto :done
)

echo  Downloading Tracktion Engine and JUCE (about 300 MB, please wait)...
git clone --depth 1 --recurse-submodules https://github.com/Tracktion/tracktion_engine.git libs\tracktion_engine
if errorlevel 1 goto :fail

:done
echo.
echo  [OK] Setup complete. Next: double-click build.bat
echo.
pause
exit /b 0

:fail
echo.
echo  Setup did not finish. Check the internet connection and run setup.bat again.
echo  If it fails again, copy the messages above and send them to Claude.
echo.
pause
exit /b 1
