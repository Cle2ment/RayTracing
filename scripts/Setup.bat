@echo off
pushd ..

:: Download premake5 beta8 if not present (Walnut-bundled version doesn't support C++23)
if not exist premake5.exe (
    echo Downloading premake5 5.0.0-beta8...
    powershell -Command "Invoke-WebRequest -Uri 'https://github.com/premake/premake-core/releases/download/v5.0.0-beta8/premake-5.0.0-beta8-windows.zip' -OutFile '%TEMP%\premake.zip'"
    if errorlevel 1 goto :download_failed
    powershell -Command "Expand-Archive -Path '%TEMP%\premake.zip' -DestinationPath '%TEMP%\premake' -Force"
    if errorlevel 1 goto :download_failed
    copy /Y "%TEMP%\premake\premake5.exe" .\premake5.exe > nul
    if errorlevel 1 goto :download_failed
    echo Done.
)

:: Verify premake5.lua exists in project root
if not exist premake5.lua (
    echo ERROR: premake5.lua not found in project root.
    echo Current directory: %CD%
    popd & pause & exit /b 1
)

.\premake5.exe vs2026
if errorlevel 1 (
    echo.
    echo ERROR: premake5 failed. Check that premake5.lua exists in the project root.
    popd & pause & exit /b 1
)
popd
pause
exit /b 0

:download_failed
echo ERROR: Failed to download premake5. Please download manually from:
echo   https://github.com/premake/premake-core/releases/tag/v5.0.0-beta8
echo   Extract premake5.exe to the project root directory.
popd & pause & exit /b 1