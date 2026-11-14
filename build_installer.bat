@echo off
setlocal enabledelayedexpansion

echo ================================================================
echo   Al-Mushaf Al-Sharif - Installer Builder
echo   Supports Windows 7, 8.1, 10, and 11 ^(both 32-bit and 64-bit^)
echo ================================================================

set "APP_DIR=%~dp0"
cd /d "%APP_DIR%"

if not exist "%APP_DIR%bin\AlMushafAlSharif-x64.exe" (
    call "%APP_DIR%build.bat"
    if errorlevel 1 exit /b 1
)
if not exist "%APP_DIR%bin\AlMushafAlSharif-win7-x86.exe" (
    call "%APP_DIR%build.bat"
    if errorlevel 1 exit /b 1
)

set "ISCC_EXE="
if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" set "ISCC_EXE=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if exist "C:\Program Files\Inno Setup 6\ISCC.exe" set "ISCC_EXE=C:\Program Files\Inno Setup 6\ISCC.exe"
if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" set "ISCC_EXE=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"

if "%ISCC_EXE%"=="" (
    echo [!] Inno Setup 6 ^(ISCC.exe^) was not found.
    exit /b 1
)

echo [*] Packaging installer via Inno Setup 6...
"%ISCC_EXE%" "%APP_DIR%installer.iss"
if errorlevel 1 exit /b 1

echo.
echo [+] Installer created successfully at:
echo     release\AlMushafAlSharif-Setup-1.0.0.exe
exit /b 0
