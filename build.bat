@echo off

@REM Invoke this with no arguments for a standard debug build
@REM Add 'release' at the end, like `build.bat release` for a release build
@REM External libraries are compiled seperately. They will be compiled if they don't exist in the build folder,
@REM but if you need to build them manually `build.bat libs` will rebuild them.


setlocal EnableDelayedExpansion

set root=%~dp0
:: remove trailing backslash
set root=%root:~0,-1%
:: working dir should be project root (same dir as this script)
pushd %root%

if not exist "build" mkdir "build"
if not exist "tools\clang\bin\clang.exe" (
    echo [First time setup] downloading clang binaries...
    call "tools\download_clang.bat"
    IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)
)

set compile_exe="%root%\tools\clang\bin\clang.exe"

if not exist "me_build.exe" (
	%compile_exe% me_build.cpp -o me_build.exe -std=c++20 -g
)
me_build.exe %compile_exe% %root% %*
:: windows weirdness, tmp file stays locked and isn't properly deleted in nob, ensure it here
del /f /q me_build.exe.old >nul 2>&1

popd


popd
endlocal