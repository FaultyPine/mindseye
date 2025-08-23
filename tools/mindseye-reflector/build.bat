::@echo off

setlocal

:: path to this bat script
set root=%~dp0
:: remove trailing backslash
set root=%root:~0,-1%
pushd %root%

for %%a in (%*) do set "%%a=1"
if not "%release%"=="1" set debug=1
if "%debug%"=="1"   set release=0 && echo [debug build]
if "%release%"=="1" set debug=0 && echo [release build]

set clang_path=%root%\..\clang
set proj_root=%root%\..\..

if "%debug%"=="1" (
    set mode_options=-DBUILD_DEBUG=1
)
if "%release%"=="1" ( 
    set mode_options=-DBUILD_DEBUG=0
)

set mindseye_options=-I%proj_root%\mindseye -DMEEXPORT
clang me_reflector.cpp -std=c++20 -g %mode_options% %mindseye_options% -o me_reflector.exe -I%proj_root% -I%clang_path%\include -L%clang_path%\lib -DCLANG_BINARIES_DIR_STR="%clang_path%\bin"

me_reflector.exe %*



popd
endlocal