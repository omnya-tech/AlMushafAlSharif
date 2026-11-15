@echo off
rem Drag a pages file (CSV) onto this file to check it
setlocal
set "ROOT=%~dp0..\.."
set "EXE=%ROOT%\bin\AlMushafAlSharif-x64.exe"
if not exist "%EXE%" set "EXE=%ROOT%\bin\AlMushafAlSharif-win7-x86.exe"
if not exist "%EXE%" (
    echo Build the program first with build.bat
    pause
    exit /b 1
)
if "%~1"=="" (
    echo Drag the pages file onto check_edition.bat
    pause
    exit /b 1
)
"%EXE%" --check-edition "%~1"
