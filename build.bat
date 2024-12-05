@echo off

setlocal

:: unpack cmd line args
for %%a in (%*) do set "%%a=1"
set builder=ninja

set root=%~dp0
:: remove trailing backslash
set root=%root:~0,-1%
if ["%builder%"]==["ninja"] (
    set build_cmd="%root%/tools/ninja/ninja.exe"
)

%build_cmd%
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)

@REM if not "%release%"=="1" set exe_path=bin\Debug
@REM if "%release%"=="1" set exe_path=bin\Release

@REM pushd %root%\%exe_path%
@REM %1.exe
@REM popd


endlocal