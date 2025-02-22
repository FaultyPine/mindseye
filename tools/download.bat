@echo off

setlocal

@REM https://github.com/llvm/llvm-project/releases/download/llvmorg-18.1.8/clang+llvm-18.1.8-x86_64-pc-windows-msvc.tar.xz



start /wait /b powershell -command "Start-BitsTransfer -Source https://github.com/llvm/llvm-project/releases/download/llvmorg-18.1.8/clang+llvm-18.1.8-x86_64-pc-windows-msvc.tar.xz -Destination tools/clang.tar.xz"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)
@REM start /wait /b powershell -command "Expand-Archive tools/clang.zip tools/clang_inner"
start /wait /b tar -zxvf "tools/clang.tar.xz" -C "tools"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)
del ".\tools\clang.tar.xz"

if exist "tools/clang+llvm-18.1.8-x86_64-pc-windows-msvc" move "tools/clang+llvm-18.1.8-x86_64-pc-windows-msvc" "tools/clang"
