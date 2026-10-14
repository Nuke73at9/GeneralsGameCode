@echo off
rem Abram's Personal Test - one-click build and install for Zero Hour and Generals.
rem Double-click this file. It builds the Release version and copies it into the game folder.
rem The first build also builds the vcpkg libraries and can take a long time (30-60 minutes).

rem Start from a clean PATH. A broken IncrediBuild entry in the system PATH makes vcvars32.bat fail.
set "PATH=C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem"

set "VSROOT=C:\Program Files\Microsoft Visual Studio\18\Community"

call "%VSROOT%\VC\Auxiliary\Build\vcvars32.bat"
if errorlevel 1 goto :fail

rem vcvars32 does not add Visual Studio's bundled CMake and Ninja when run this way, so add them here.
set "PATH=%PATH%;%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"

rem GeneralsX gets some libraries (curl, GameNetworkingSockets, ...) through vcpkg.
rem Use VCPKG_ROOT if it is set, otherwise the vcpkg that comes with Visual Studio.
if not defined VCPKG_ROOT if exist "%VSROOT%\VC\vcpkg\vcpkg.exe" set "VCPKG_ROOT=%VSROOT%\VC\vcpkg"
if not defined VCPKG_ROOT goto :novcpkg
if not exist "%VCPKG_ROOT%\vcpkg.exe" goto :novcpkg
echo Using vcpkg from %VCPKG_ROOT%

cd /d "%~dp0"

echo.
echo === Building (Release) ===
cmake --workflow --preset abram-win32
if errorlevel 1 goto :fail

echo.
echo === Installing into the game folder ===
cmake --install build/abram-win32 --config Release
if errorlevel 1 goto :fail

echo.
echo === Done. Launch generalszh.exe from your Zero Hour folder. ===
pause
exit /b 0

:novcpkg
echo.
echo === vcpkg was not found. ===
echo Open the Visual Studio Installer, click Modify, go to Individual components,
echo tick "vcpkg package manager", click Modify, then run this script again.
pause
exit /b 1

:fail
echo.
echo === Build FAILED. Copy the error lines above and send them to Claude. ===
pause
exit /b 1
