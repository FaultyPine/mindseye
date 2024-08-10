@echo off

setlocal

set root=%~dp0
:: remove trailing backslash
set root=%root:~0,-1%
set premake_cmd="%root%/tools/premake/premake5.exe"
%premake_cmd% --file=premake5.lua --cc=clang gmake2


endlocal