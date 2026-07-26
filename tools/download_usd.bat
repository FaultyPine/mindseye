@echo off

set USD_DIR=mindseye\external\usd
set USD_ZIP=%USD_DIR%\usd.zip
set USD_URL=https://developer.nvidia.com/downloads/usd/usd_binaries/25.08/usd.py312.windows-x86_64.usdview.release-v25.08.71e038c1.zip

if not exist "%USD_DIR%" mkdir "%USD_DIR%"
curl -L -o "%USD_ZIP%" "%USD_URL%"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)

tar -xf "%USD_ZIP%" -C "%USD_DIR%"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)

del "%USD_ZIP%"
