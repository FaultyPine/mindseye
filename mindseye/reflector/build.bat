::@echo off

setlocal

:: path to this bat script
set root=%~dp0
:: remove trailing backslash
set root=%root:~0,-1%
pushd %root%

set proj_root=%root%\..\..

set clang_path=%proj_root%\tools\clang

set mindseye_options=-I%proj_root%\mindseye -DMEEXPORT
set clang_options=-I%clang_path%\include -L%clang_path%\lib -llibclang -DCLANG_BINARIES_DIR_STR="%clang_path%\bin"

clang me_reflector.cpp -Wall -std=c++20 -g %mindseye_options% -o me_reflector.exe -I%proj_root% %clang_options%
if %ERRORLEVEL% NEQ 0 (echo [Reflector compile] Error:%ERRORLEVEL% && exit /b)

me_reflector.exe %1 %proj_root%\mindseye %proj_root%\mindseye\generatedtypes
if %ERRORLEVEL% NEQ 0 (echo [Reflector run] Error:%ERRORLEVEL% && exit /b)


popd
endlocal