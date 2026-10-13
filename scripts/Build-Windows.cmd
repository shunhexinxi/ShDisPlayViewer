@echo off
setlocal
if "%QT_ROOT%"=="" (echo Set QT_ROOT to the Qt 6.11 MSVC x64 directory & exit /b 1)
if "%VCPKG_ROOT%"=="" (echo Set VCPKG_ROOT & exit /b 1)
set "SHDISPLAY_VCPKG_ROOT=%VCPKG_ROOT%"
if not defined VSINSTALLDIR (
  for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath`) do set "SHDISPLAY_VS=%%i"
)
if defined SHDISPLAY_VS call "%SHDISPLAY_VS%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
set "VCPKG_ROOT=%SHDISPLAY_VCPKG_ROOT%"
if not defined VSINSTALLDIR exit /b 1
set "PATH=%QT_ROOT%\bin;%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
cd /d "%~dp0.."
cmake --preset windows-release %*
if errorlevel 1 exit /b 1
cmake --build --preset windows-release --parallel 6
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\Deploy-Windows.ps1 -QtRoot "%QT_ROOT%" -VcpkgRoot "%VCPKG_ROOT%" -OutputDirectory "%CD%\build\windows-release"
if errorlevel 1 exit /b 1
ctest --preset windows-release
exit /b %ERRORLEVEL%
