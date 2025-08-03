@echo off

setlocal

@REM https://github.com/llvm/llvm-project/releases/download/llvmorg-18.1.8/clang+llvm-18.1.8-x86_64-pc-windows-msvc.tar.xz


if not exist "tools/clang.tar.xz" (
    powershell -command "Invoke-WebRequest -Uri https://github.com/llvm/llvm-project/releases/download/llvmorg-18.1.8/clang+llvm-18.1.8-x86_64-pc-windows-msvc.tar.xz -OutFile tools/clang.tar.xz"
    IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)
)
tar -zxvf "tools/clang.tar.xz" -C "tools"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)
if exist "tools/clang+llvm-18.1.8-x86_64-pc-windows-msvc" move "tools/clang+llvm-18.1.8-x86_64-pc-windows-msvc" "tools/clang"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)
del "tools\clang.tar.xz"

