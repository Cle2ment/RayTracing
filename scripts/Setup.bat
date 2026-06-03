@echo off
pushd %~dp0..
echo [Setup] Working: %CD%
if exist vendor\ispc\bin\ispc.exe (echo [Setup] ISPC found) else (echo [Setup] ISPC not found)
echo [Setup] Starting xmake build...
xmake f -m release
if errorlevel 1 (pause & exit /b 1)
xmake build
if errorlevel 1 (pause & exit /b 1)
echo ============================================
echo [Setup] Build successful.
echo ============================================
popd
pause
