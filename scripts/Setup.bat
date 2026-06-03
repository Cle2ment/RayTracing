@echo off
cd /d "%~dp0.."

:: ── Download ISPC if not present (optional CPU SIMD acceleration) ──
if not exist vendor\ispc\bin\ispc.exe (
    echo [Setup] Downloading ISPC v1.30.0...
    powershell -Command "Invoke-WebRequest -Uri 'https://github.com/ispc/ispc/releases/download/v1.30.0/ispc-v1.30.0-windows.zip' -OutFile '%TEMP%\ispc.zip'"
    if errorlevel 1 goto :ispc_skip
    powershell -Command "Expand-Archive -Path '%TEMP%\ispc.zip' -DestinationPath '%TEMP%\ispc_tmp' -Force"
    if errorlevel 1 goto :ispc_skip
    if not exist vendor\ispc mkdir vendor\ispc
    xcopy /E /Y "%TEMP%\ispc_tmp\ispc-v1.30.0-windows\*" vendor\ispc\ > nul
    if errorlevel 1 goto :ispc_skip
    echo [Setup] ISPC installed.
    goto :ispc_done
    :ispc_skip
    echo [Setup] ISPC skipped. CPU path tracer will use C++ fallback.
    echo         Manual install: https://github.com/ispc/ispc/releases/tag/v1.30.0
    :ispc_done
) else (
    echo [Setup] ISPC found.
)

:: ── Build with xmake ──
echo [Setup] Configuring and building with xmake...
xmake f -m release
if errorlevel 1 (
    echo [Setup] ERROR: xmake configure failed.
    pause & exit /b 1
)

xmake build
if errorlevel 1 (
    echo [Setup] ERROR: xmake build failed.
    pause & exit /b 1
)

echo ============================================
echo [Setup] Build successful. Run with:
echo   xmake run RayTracing
echo ============================================
pause
exit /b 0
