@echo off


powershell -command "Invoke-WebRequest -Uri https://github.com/FaultyPine/mindseye/releases/download/libs/bin.zip -OutFile mindseye/external/bgfx/bin.zip"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)
@REM start /wait /b powershell -command "Expand-Archive tools/clang.zip tools/clang_inner"
tar -zxvf "mindseye/external/bgfx/bin.zip" -C "mindseye/external/bgfx"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)
del "mindseye\external\bgfx\bin.zip"

