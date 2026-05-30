@echo off
pushd ..

:: Download premake5 beta8 if not present (Walnut-bundled version doesn't support C++23)
if not exist premake5.exe (
    echo Downloading premake5 5.0.0-beta8...
    powershell -Command "Invoke-WebRequest -Uri 'https://github.com/premake/premake-core/releases/download/v5.0.0-beta8/premake-5.0.0-beta8-windows.zip' -OutFile '%TEMP%\premake.zip'" && powershell -Command "Expand-Archive -Path '%TEMP%\premake.zip' -DestinationPath '%TEMP%\premake' -Force" && copy /Y "%TEMP%\premake\premake5.exe" premake5.exe > nul
    if errorlevel 1 (
        echo ERROR: Failed to download premake5. Please download manually from:
        echo   https://github.com/premake/premake-core/releases/tag/v5.0.0-beta8
        popd & pause & exit /b 1
    )
    echo Done.
)

premake5.exe vs2026
popd
pause