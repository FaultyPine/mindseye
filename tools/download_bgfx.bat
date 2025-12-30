@echo off


curl -L -o mindseye/external/bgfx/bin.zip https://github.com/FaultyPine/mindseye/releases/download/libs/bin.zip
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)
@REM start /wait /b powershell -command "Expand-Archive tools/clang.zip tools/clang_inner"
tar -zxvf "mindseye/external/bgfx/bin.zip" -C "mindseye/external/bgfx"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)
del "mindseye\external\bgfx\bin.zip"

