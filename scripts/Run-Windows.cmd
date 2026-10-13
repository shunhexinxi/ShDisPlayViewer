@echo off
setlocal
cd /d "%~dp0.."
start "ShDisplayViewer" "build\windows-release\ShDisplayViewer.exe" %*
