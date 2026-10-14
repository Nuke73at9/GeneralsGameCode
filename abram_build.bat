@echo off
rem Abram's Personal Test - one-click build and install for Zero Hour and Generals.
rem Double-click this file. It builds the Release version and copies it into the game folder.

rem Start from a clean PATH. A broken IncrediBuild entry in the system PATH makes vcvars32.bat fail.
set "PATH=C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem"

set "VSROOT=C:\Program Files\Microsoft Visual Studio\18\Community"

call "%VSROOT%\VC\Auxiliary\Build\vcvars32.bat"
if errorlevel 1 goto :fail

rem vcvars32 does not add Visual Studio's bundled CMake and Ninja when run this way, so add them here.
set "PATH=%PATH%;%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"

cd /d "%~dp0"

echo.
echo === Building (Release) ===
cmake --workflow --preset win32
if errorlevel 1 goto :fail

echo.
echo === Installing into the game folder ===
cmake --install build/win32 --config Release
if errorlevel 1 goto :fail

echo.
echo === Done. Launch generalszh.exe from your Zero Hour folder. ===
pause
exit /b 0

:fail
echo.
echo === Build FAILED. Copy the error lines above and send them to Claude. ===
pause
exit /b 1
