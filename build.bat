@echo off

setlocal ENABLEDELAYEDEXPANSION

set root=%~dp0
:: remove trailing backslash
set root=%root:~0,-1%
:: working dir should be project root (same dir as this script)
pushd %root%

if not exist "build" mkdir "build"
if not exist "tools\clang" (
    echo [First time setup] downloading clang binaries...
    call "tools\download_clang.bat"
)
if not exist "build\vulkan-1.dll" (
    if not exist "mindseye\external\vulkan_lib\Lib" (
        echo [First time setup] downloading vulkan sdk...
        call "tools\download_vulkan.bat"
    )
    copy "mindseye\external\vulkan_lib\Lib\vulkan-1.dll" "build"
)
if not exist "mindseye\external\bgfx\bin" (
    echo [Build setup] Downloading bgfx binaries...
    call "tools\download_bgfx.bat"
)
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)
set clang_bin_dir=tools\clang\bin
set "PATH=%PATH%;%root%\%clang_bin_dir%"

:: unpack cmd line args
for %%a in (%*) do set "%%a=1"
if not "%release%"=="1" set debug=1
if "%debug%"=="1"   set release=0 && echo [debug mode]
if "%release%"=="1" set debug=0 && echo [release mode]
if "%~1"=="" echo [full build] && set "mindseye=1" && set "testbed=1" && set "driver=1"


:: recursive fs search for specified file extensions
:: dir /b /s | findstr /e /l ".cpp .c"
::FOR /F "delims=" %%F IN ('dir /b /s ^| findstr /e /l ".cpp .c"') DO (
::    SET sources=!sources!%%F,
::)
::echo SOURCES: %sources%

@REM currently assuming we do compile+link all in one step, this may change.

@REM common compile flags
set compile_flags_common= -I%root% -I%root%\mindseye -I%root%/mindseye/external %include_libs% -std=c++20 -DNOMINMAX -DUNICODE -Wno-deprecated-declarations -g -gcodeview -gno-column-info -Wall -Wextra -Wno-unused-parameter -ferror-limit=10000
for /f %%i in ('call git describe --always --dirty')   do set compile_flags_common=%compile_flags_common% -DBUILD_GIT_HASH=\"%%i\"
set linker_flags_common= -luser32 -lgdi32 -Wl,-subsystem:windows -fuse-ld=lld 

:: mindseye engine
set link_mindseye_libs_rel= -L%root%\mindseye\external\bgfx\bin -lbgfxRelease -lbimgRelease -lbxRelease
set link_mindseye_libs_dbg= -L%root%\mindseye\external\bgfx\bin -lbgfxDebug -lbimgDebug -lbxDebug
set compile_mindseye_dbg= -O0 -DBUILD_DEBUG=1 -DMEEXPORT -D_USRDLL -D_WINDLL -D_DLL -shared -D_DEBUG
set compile_mindseye_rel= -O2 -DBUILD_DEBUG=0 -DMEEXPORT -D_USRDLL -D_WINDLL -D_DLL -shared
set link_mindseye_rel= %linker_flags_common% %link_mindseye_libs_rel% 
set link_mindseye_dbg= %linker_flags_common% %link_mindseye_libs_dbg% 


:: testbed
set compile_testbed_dbg= -O0 -DBUILD_DEBUG=1 -D_USRDLL -D_WINDLL -D_DLL -shared
set compile_testbed_rel= -O2 -DBUILD_DEBUG=0 -D_USRDLL -D_WINDLL -D_DLL -shared
set link_testbed=  -L%root%\build -lmindseye %linker_flags_common% %linker_flags_common% -Wl,/DLL

::driver
::BOOKMARK(create main driver program that loads engine and game dlls, then initializes engine to point to the game)
:: that way, everything can be dll reloaded, and the editor can also be loaded from the engine to have context of the game
set compile_driver_dbg= -O0 -DBUILD_DEBUG=1
set compile_driver_rel= -O2 -DBUILD_DEBUG=0
set link_driver= -L%root%\build -lmindseye %linker_flags_common%

set compile_exe="%root%\tools\clang\bin\clang.exe"
if "%debug%"=="1" (
    set compile_mindseye=%compile_exe% %compile_mindseye_dbg% %compile_flags_common% %link_mindseye_dbg%
    set compile_testbed=%compile_exe% %compile_testbed_dbg% %compile_flags_common% %link_testbed%
    set compile_driver=%compile_exe% %compile_driver_dbg% %compile_flags_common% %link_driver%
)
if "%release%"=="1" ( 
    set compile_mindseye=%compile_exe% %compile_mindseye_rel% %compile_flags_common% %link_mindseye_rel%
    set compile_testbed=%compile_exe% %compile_testbed_rel% %compile_flags_common% %link_testbed%
    set compile_driver=%compile_exe% %compile_driver_rel% %compile_flags_common% %link_driver%
)

if not exist build mkdir build
pushd build

if "%mindseye%"=="1" echo [mindseye compile] && %compile_mindseye% %root%\mindseye\me_unity.cpp -o mindseye.dll
IF %ERRORLEVEL% NEQ 0 (echo [mindseye compile] Error:%ERRORLEVEL% && exit /b)
if "%testbed%"=="1" echo [testbed compile] && %compile_testbed% %root%\projects\testbed\testbed.cpp -o testbed.dll
IF %ERRORLEVEL% NEQ 0 (echo [testbed compile] Error:%ERRORLEVEL% && exit /b)
if not exist "%root%\build\driver.exe" (
    if "%driver%"=="1" echo [driver compile] && %compile_driver% %root%\mindseye\platform\driver.cpp -o driver.exe
    IF %ERRORLEVEL% NEQ 0 (echo [driver compile] Error:%ERRORLEVEL% && exit /b)
)
echo Successfully built

if "%run%"=="1" echo Running... && call testbed.exe

popd


popd
endlocal