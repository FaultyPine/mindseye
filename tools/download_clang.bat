@echo off


@REM https://github.com/llvm/llvm-project/releases/download/llvmorg-18.1.8/clang+llvm-18.1.8-x86_64-pc-windows-msvc.tar.xz


if not exist "tools/clang.zip" (
    powershell -command "Invoke-WebRequest -Uri https://github.com/FaultyPine/mindseye/releases/download/libs/clang.zip -OutFile tools/clang.zip"
    IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)
)
powershell -command "Expand-Archive tools/clang.zip -DestinationPath tools"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)
del "tools\clang.zip"
