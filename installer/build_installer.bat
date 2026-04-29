@echo off
REM Build the XYZPan installer. Requires Inno Setup 6 installed.
REM Run from the repository root after building the Release config:
REM   cmake --build build --config Release
REM   installer\build_installer.bat

set ISCC="C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if not exist %ISCC% (
    echo Inno Setup 6 not found at %ISCC%
    echo Install from https://jrsoftware.org/issetup.exe
    exit /b 1
)

%ISCC% "%~dp0XYZPan.iss"
if %ERRORLEVEL% neq 0 (
    echo Installer build failed.
    exit /b 1
)

echo.
echo Installer built: installer\output\XYZPan-1.0.0-Setup.exe
