@echo off
pushd %~dp0..
echo [Setup] Working: %CD%

:: Check submodule
if not exist Walnut\Walnut\src\Walnut\Application.cpp (
    echo [Setup] Walnut submodule not found, running git submodule update --init...
    git submodule update --init --recursive
    if errorlevel 1 (pause & exit /b 1)
)

if exist vendor\ispc\bin\ispc.exe (echo [Setup] ISPC found) else (echo [Setup] ISPC not found)
echo [Setup] Starting xmake build...
xmake f -m release
if errorlevel 1 (pause & exit /b 1)
xmake build
if errorlevel 1 (pause & exit /b 1)
echo [Setup] Generating Visual Studio solution...
xmake project -k vsxmake -y -m release
if errorlevel 1 (pause & exit /b 1)
echo [Setup] Converting .sln to .slnx...
dotnet sln vsxmake2026\RayTracing.sln migrate
if not errorlevel 1 (
    del /Q vsxmake2026\RayTracing.sln >nul 2>&1
    echo [Setup] .slnx created in vsxmake2026\
) else (
    echo [Setup] dotnet sln migrate failed, keeping .sln.
)
echo ============================================
echo [Setup] Build successful.
echo [Setup] VS Solution: vsxmake2026\RayTracing.slnx
echo ============================================
popd
pause
