@echo off

setlocal ENABLEDELAYEDEXPANSION

set root=%~dp0
:: remove trailing backslash
set root=%root:~0,-1%
:: working dir should be project root (same dir as this script)
pushd %root%

if not exist "tools\clang" (
    echo [First time setup] downloading clang binaries...
    call "tools/download.bat"
)
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)
set clang_bin_dir=tools\clang\bin
set "PATH=%PATH%;%root%\%clang_bin_dir%"

:: unpack cmd line args
for %%a in (%*) do set "%%a=1"
if not "%release%"=="1" set debug=1
if "%debug%"=="1"   set release=0 && echo [debug mode]
if "%release%"=="1" set debug=0 && echo [release mode]
if "%~1"=="" echo [full build] && set "mindseye=1" && set "testbed=1"


:: recursive fs search for specified file extensions
:: dir /b /s | findstr /e /l ".cpp .c"
::FOR /F "delims=" %%F IN ('dir /b /s ^| findstr /e /l ".cpp .c"') DO (
::    SET sources=!sources!%%F,
::)
::echo SOURCES: %sources%

@REM currently assuming we do compile+link all in one step, this may change.

@REM common compile flags
set compile_flags_common= -I%root% -I%root%\mindseye -DOS_WINDOWS -DCOMPILER_CLANG -D_UNICODE -g -gcodeview -gno-column-info -std=c++20 -Wall -ferror-limit=10000
for /f %%i in ('call git describe --always --dirty')   do set compile_flags_common=%compile_flags_common% -DBUILD_GIT_HASH=\"%%i\"
set linker_flags_common= -luser32 -Wl,-subsystem:console

:: mindseye engine
set compile_mindseye_dbg= -O0 -DBUILD_DEBUG=1 -DMEEXPORT -D_USRDLL -D_WINDLL -D_DLL -shared
set compile_mindseye_rel= -O2 -DBUILD_DEBUG=0 -DMEEXPORT -D_USRDLL -D_WINDLL -D_DLL -shared 
set link_mindseye= %linker_flags_common% -Wl,msvcrt.lib,/DLL,/NODEFAULTLIB:libcmt.lib,/NODEFAULTLIB:libcmtd.lib,/NODEFAULTLIB:msvcrtd.lib

:: testbed
set compile_testbed_dbg= -O0 -DBUILD_DEBUG=1 
set compile_testbed_rel= -O2 -DBUILD_DEBUG=0
set link_testbed=  -L%root%\build -lmindseye %linker_flags_common%

if "%debug%"=="1" (
    set compile_mindseye=call clang %compile_mindseye_dbg% %compile_flags_common% %link_mindseye%
    set compile_testbed=call clang %compile_testbed_dbg% %compile_flags_common% %link_testbed%
)
if "%release%"=="1" ( 
    set compile_mindseye=call clang %compile_mindseye_rel% %compile_flags_common% %link_mindseye%
    set compile_testbed=call clang %compile_testbed_rel% %compile_flags_common% %link_testbed%
)

if not exist build mkdir build
pushd build

if "%mindseye%"=="1" echo [mindseye compile] && %compile_mindseye% %root%\mindseye\me_unity.cpp -o mindseye.dll
IF %ERRORLEVEL% NEQ 0 (echo [mindseye compile] Error:%ERRORLEVEL% && exit /b)
if "%testbed%"=="1" echo [testbed compile] && %compile_testbed% %root%\projects\testbed\testbed.cpp -o testbed.exe
IF %ERRORLEVEL% NEQ 0 (echo [testbed compile] Error:%ERRORLEVEL% && exit /b)
echo Successfully built

if "%run%"=="1" echo Running... && call testbed.exe

popd


popd
endlocal