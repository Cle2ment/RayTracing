@echo off
cd /d "%~dp0.."
echo [Setup] Project root: %CD%
if not exist vendor\premake\premake5.exe (echo [Setup] Downloading premake5... & powershell -Command "Invoke-WebRequest -Uri 'https://github.com/premake/premake-core/releases/download/v5.0.0-beta8/premake-5.0.0-beta8-windows.zip' -OutFile '%TEMP%\premake.zip'" & powershell -Command "Expand-Archive -Path '%TEMP%\premake.zip' -DestinationPath '%TEMP%\premake_tmp' -Force" & if not exist vendor\premake mkdir vendor\premake & copy /Y "%TEMP%\premake_tmp\premake5.exe" vendor\premake\premake5.exe > nul & echo [Setup] premake5 installed.) else (echo [Setup] premake5 found.)
if not exist vendor\ispc\bin\ispc.exe (echo [Setup] Downloading ISPC... & powershell -Command "Invoke-WebRequest -Uri 'https://github.com/ispc/ispc/releases/download/v1.30.0/ispc-v1.30.0-windows.zip' -OutFile '%TEMP%\ispc.zip'" & powershell -Command "Expand-Archive -Path '%TEMP%\ispc.zip' -DestinationPath '%TEMP%\ispc_tmp' -Force" & if not exist vendor\ispc mkdir vendor\ispc & xcopy /E /Y "%TEMP%\ispc_tmp\ispc-v1.30.0-windows\*" vendor\ispc\ > nul & echo [Setup] ISPC installed.) else (echo [Setup] ISPC found.)
echo [Setup] Generating VS solution...
vendor\premake\premake5.exe vs2026
echo ============================================
echo [Setup] Completed. Open RayTracing.slnx to build.
echo ============================================
pause
