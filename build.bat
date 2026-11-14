@echo off
setlocal enabledelayedexpansion

echo ================================================================
echo   Al-Mushaf Al-Sharif - Native C++ Builder (Win7 / 8.1 / 10 / 11)
echo ================================================================

set "ROOT_DIR=%~dp0"
if not exist "%ROOT_DIR%bin" mkdir "%ROOT_DIR%bin"
if not exist "%ROOT_DIR%build" mkdir "%ROOT_DIR%build"

set "SRC_FILES="%ROOT_DIR%src\main.cpp" "%ROOT_DIR%src\database.cpp""
set "INC_FLAGS=-I"%ROOT_DIR%src" -I"%ROOT_DIR%third_party\sqlite3""
set "SQLITE_DEFS=-DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION=1"
set "WIN_LIBS=-lgdi32 -luser32 -lkernel32 -lshell32 -lole32 -loleaut32 -luuid -lwinmm -lcomctl32 -ladvapi32"

rem ---------------------------------------------------------------
rem  1) Zig (preferred): builds both 64-bit and 32-bit (Windows 7+)
rem     Set ZIG_EXE to a full path to use a Zig that is not on PATH.
rem ---------------------------------------------------------------
if "%ZIG_EXE%"=="" (
    where zig >nul 2>nul && set "ZIG_EXE=zig"
)

if not "%ZIG_EXE%"=="" (
    echo [*] Using Zig C/C++ compiler: "%ZIG_EXE%"

    if not exist "%ROOT_DIR%build\sqlite3_x64.o" (
        echo [*] Compiling SQLite3 for 64-bit ^(x64^)...
        "%ZIG_EXE%" cc -target x86_64-windows-gnu -O2 %SQLITE_DEFS% -c "%ROOT_DIR%third_party\sqlite3\sqlite3.c" -o "%ROOT_DIR%build\sqlite3_x64.o"
        if errorlevel 1 exit /b 1
    )
    echo [*] Building 64-bit executable ^(AlMushafAlSharif-x64.exe^)...
    "%ZIG_EXE%" c++ -target x86_64-windows-gnu -std=c++17 -O2 -s -municode -Wl,--subsystem,windows %INC_FLAGS% %SRC_FILES% "%ROOT_DIR%src\app.rc" "%ROOT_DIR%build\sqlite3_x64.o" -o "%ROOT_DIR%bin\AlMushafAlSharif-x64.exe" %WIN_LIBS%
    if errorlevel 1 exit /b 1

    if not exist "%ROOT_DIR%build\sqlite3_x86.o" (
        echo [*] Compiling SQLite3 for 32-bit ^(Windows 7+ x86^)...
        "%ZIG_EXE%" cc -target x86-windows-gnu -O2 %SQLITE_DEFS% -c "%ROOT_DIR%third_party\sqlite3\sqlite3.c" -o "%ROOT_DIR%build\sqlite3_x86.o"
        if errorlevel 1 exit /b 1
    )
    echo [*] Building 32-bit Windows 7+ executable ^(AlMushafAlSharif-win7-x86.exe^)...
    "%ZIG_EXE%" c++ -target x86-windows-gnu -std=c++17 -O2 -s -municode -Wl,--subsystem,windows %INC_FLAGS% %SRC_FILES% "%ROOT_DIR%src\app.rc" "%ROOT_DIR%build\sqlite3_x86.o" -o "%ROOT_DIR%bin\AlMushafAlSharif-win7-x86.exe" %WIN_LIBS%
    if errorlevel 1 exit /b 1

    echo [+] Build succeeded in: "%ROOT_DIR%bin"
    exit /b 0
)

rem ---------------------------------------------------------------
rem  2) MinGW-w64 g++ (architecture follows the installed toolchain)
rem ---------------------------------------------------------------
where g++ >nul 2>nul
if %errorlevel%==0 (
    echo [*] Using MinGW g++ compiler...
    set "OUT_NAME=AlMushafAlSharif-x64.exe"
    for /f "delims=" %%m in ('g++ -dumpmachine') do echo %%m | findstr /i "i686 i386" >nul && set "OUT_NAME=AlMushafAlSharif-win7-x86.exe"
    if not exist "%ROOT_DIR%build\sqlite3.o" (
        gcc -O2 %SQLITE_DEFS% -c "%ROOT_DIR%third_party\sqlite3\sqlite3.c" -o "%ROOT_DIR%build\sqlite3.o"
        if errorlevel 1 exit /b 1
    )
    windres -I"%ROOT_DIR%src" "%ROOT_DIR%src\app.rc" -O coff -o "%ROOT_DIR%build\app_res.o"
    if errorlevel 1 exit /b 1
    g++ -std=c++17 -O2 -s -municode -mwindows -static %INC_FLAGS% %SRC_FILES% "%ROOT_DIR%build\app_res.o" "%ROOT_DIR%build\sqlite3.o" -o "%ROOT_DIR%bin\!OUT_NAME!" %WIN_LIBS%
    if errorlevel 1 exit /b 1
    echo [+] Build succeeded in: "%ROOT_DIR%bin\!OUT_NAME!"
    exit /b 0
)

rem ---------------------------------------------------------------
rem  3) Microsoft Visual C++ (run from a "Developer Command Prompt")
rem ---------------------------------------------------------------
where cl >nul 2>nul
if %errorlevel%==0 (
    echo [*] Using Microsoft Visual C++ ^(cl.exe^)...
    set "OUT_NAME=AlMushafAlSharif-x64.exe"
    if /i "%VSCMD_ARG_TGT_ARCH%"=="x86" set "OUT_NAME=AlMushafAlSharif-win7-x86.exe"
    rc /nologo /fo "%ROOT_DIR%build\app.res" "%ROOT_DIR%src\app.rc"
    if errorlevel 1 exit /b 1
    cl /nologo /O2 /EHsc /utf-8 /std:c++17 /DUNICODE /D_UNICODE /DNOMINMAX /DSQLITE_THREADSAFE=0 /DSQLITE_OMIT_LOAD_EXTENSION=1 /I"%ROOT_DIR%src" /I"%ROOT_DIR%third_party\sqlite3" %SRC_FILES% "%ROOT_DIR%third_party\sqlite3\sqlite3.c" "%ROOT_DIR%build\app.res" /Fo"%ROOT_DIR%build\\" /Fe:"%ROOT_DIR%bin\!OUT_NAME!" /link /SUBSYSTEM:WINDOWS gdi32.lib user32.lib kernel32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib winmm.lib comctl32.lib advapi32.lib
    if errorlevel 1 exit /b 1
    echo [+] Build succeeded in: "%ROOT_DIR%bin\!OUT_NAME!"
    exit /b 0
)

echo [!] No C++ compiler found in PATH ^(zig, MinGW g++, or MSVC cl.exe^).
exit /b 1
