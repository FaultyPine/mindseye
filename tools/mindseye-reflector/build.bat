::@echo off

setlocal

:: path to this bat script
set root=%~dp0
:: remove trailing backslash
set root=%root:~0,-1%
pushd %root%

set clang_path=%root%\..\clang
set proj_root=%root%\..\..

set mindseye_options=-I%proj_root%\mindseye -DMEEXPORT
clang me_reflector.cpp -std=c++20 -g %mindseye_options% -o me_reflector.exe -I%proj_root% -I%clang_path%\include -L%clang_path%\lib -DCLANG_BINARIES_DIR_STR="%clang_path%\bin"

me_reflector.exe %*



popd
endlocal