@echo off

setlocal

set root=%~dp0
:: remove trailing backslash
set root=%root:~0,-1%
set make_cmd="%root%/tools/make/bin/make.exe"

:: unpack cmd line args
for %%a in (%*) do set "%%a=1"


%make_cmd% %*


endlocal