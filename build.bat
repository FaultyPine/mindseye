@echo off


@REM Invoke this with no arguments for a standard debug build
@REM Add 'release' at the end, like `build.bat release` for a release build
@REM External libraries are compiled seperately. They will be compiled if they don't exist in the build folder,
@REM but if you need to build them manually `build.bat libs` will rebuild them.


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

set "driver=0" 
if not exist "build\driver.exe" (
    set "driver=1" 
)

:: unpack cmd line args
for %%a in (%*) do set "%%a=1"
if not "%release%"=="1" set debug=1
if "%debug%"=="1"   set release=0 && echo [debug mode]
if "%release%"=="1" set debug=0 && echo [release mode]
if "%~1"=="" echo [full build] && set "mindseye=1" && set "testbed=1"
if not exist "build\mindseye_ext.lib" (
    set "libs=1"
)

:: recursive fs search for specified file extensions
:: dir /b /s | findstr /e /l ".cpp .c"
::FOR /F "delims=" %%F IN ('dir /b /s ^| findstr /e /l ".cpp .c"') DO (
::    SET sources=!sources!%%F,
::)
::echo SOURCES: %sources%

@REM currently assuming we do compile+link all in one step, this may change.
set include_libs=-I%root%\mindseye\external\imgui -I%root%\mindseye\external\bgfx\bgfx\include -I%root%\mindseye\external\bgfx\bgfx\3rdparty -I%root%\mindseye\external\bgfx\bx\include -I%root%\mindseye\external\bgfx\bimg\include

@REM common compile flags
set app_flags=-DSHIPPING_BUILD=0 -ftime-trace
set compile_flags_common=%app_flags% -I%root% -I%root%\mindseye -I%root%\mindseye\external %include_libs% -std=c++20 -DNOMINMAX -DUNICODE -Wno-deprecated-declarations -g -gcodeview -gno-column-info -Wall -Wextra -Wno-unused-parameter -ferror-limit=500
for /f %%i in ('call git describe --always --dirty')   do set compile_flags_common=%compile_flags_common% -DBUILD_GIT_HASH=\"%%i\"
set linker_flags_common=-luser32 -lgdi32 -fuse-ld=lld-link


:: external libraries
set link_bgfx_libs_rel= -L%root%\mindseye\external\bgfx\bin -lbgfxRelease -lbimgRelease -lbxRelease
set link_bgfx_libs_dbg= -L%root%\mindseye\external\bgfx\bin -lbgfxDebug -lbimgDebug -lbxDebug

set compile_ext_libs_dbg=-O0 -DBUILD_DEBUG=1 -DMEEXPORT -DBX_CONFIG_DEBUG=1 -shared -D_DEBUG 
set compile_ext_libs_rel=-O2 -DBUILD_DEBUG=0 -DMEEXPORT -DBX_CONFIG_DEBUG=0 -shared
set link_ext_libs_dbg=%linker_flags_common% 
set link_ext_libs_rel=%linker_flags_common%


:: mindseye engine
set compile_mindseye_dbg= -O0 -DBUILD_DEBUG=1 -DMEEXPORT -D_USRDLL -D_WINDLL -D_DLL -shared -DBX_CONFIG_DEBUG=1 -D_DEBUG
set compile_mindseye_rel= -O2 -DBUILD_DEBUG=0 -DMEEXPORT -D_USRDLL -D_WINDLL -D_DLL -shared -DBX_CONFIG_DEBUG=0
set link_mindseye_rel= %linker_flags_common% %link_bgfx_libs_rel% -lmindseye_ext
set link_mindseye_dbg= %linker_flags_common% %link_bgfx_libs_dbg% -lmindseye_ext

:: mindseye shaders
set compile_mindseye_shader_fs=%root%\mindseye\external\bgfx\bin\shadercDebug.exe -f %root%\mindseye\shaders\fs_rect.sc -o %root%\mindseye\shaders\fs_rect.h --bin2c --platform windows --type fragment -p 440 --varyingdef %root%\mindseye\shaders\rect.def.sc
set compile_mindseye_shader_vs=%root%\mindseye\external\bgfx\bin\shadercDebug.exe -f %root%\mindseye\shaders\vs_rect.sc -o %root%\mindseye\shaders\vs_rect.h --bin2c --platform windows --type vertex -p 440 --varyingdef %root%\mindseye\shaders\rect.def.sc
set compile_mindseye_shaders=%compile_mindseye_shader_fs% && %compile_mindseye_shader_vs%

:: testbed
set compile_testbed_dbg= -O0 -DBUILD_DEBUG=1 -D_USRDLL -D_WINDLL -D_DLL -shared
set compile_testbed_rel= -O2 -DBUILD_DEBUG=0 -D_USRDLL -D_WINDLL -D_DLL -shared
set link_testbed=  -L%root%\build -lmindseye %linker_flags_common% %linker_flags_common%

::driver
set compile_driver_dbg= -O0 -DBUILD_DEBUG=1
set compile_driver_rel= -O2 -DBUILD_DEBUG=0
set link_driver= -L%root%\build -lmindseye %linker_flags_common% -Wl,/subsystem:windows


set compile_exe="%root%\tools\clang\bin\clang.exe"
if "%debug%"=="1" (
    set compile_mindseye=%compile_exe% %compile_mindseye_dbg% %compile_flags_common% %link_mindseye_dbg%
    set compile_testbed=%compile_exe% %compile_testbed_dbg% %compile_flags_common% %link_testbed%
    set compile_driver=%compile_exe% %compile_driver_dbg% %compile_flags_common% %link_driver%
    set compile_external_libraries=%compile_exe% %compile_ext_libs_dbg% %compile_flags_common% %link_ext_libs_dbg%
)
if "%release%"=="1" ( 
    set compile_mindseye=%compile_exe% %compile_mindseye_rel% %compile_flags_common% %link_mindseye_rel%
    set compile_testbed=%compile_exe% %compile_testbed_rel% %compile_flags_common% %link_testbed%
    set compile_driver=%compile_exe% %compile_driver_rel% %compile_flags_common% %link_driver%
    set compile_external_libraries=%compile_exe% %compile_ext_libs_rel% %compile_flags_common% %link_ext_libs_rel%
)
set external_lib_postprocess=call "%root%\tools\dll2lib.bat" 64 %root%\build\mindseye_ext.dll

if not exist build mkdir build
pushd build

if "%libs%"=="1" echo [External libraries compile] && %compile_external_libraries% %root%\mindseye\me_external_unity.cpp -o mindseye_ext.dll
if %ERRORLEVEL% NEQ 0 (echo [External libraries compile] Error:%ERRORLEVEL% && exit /b)

if "%mindseye%"=="1" echo [mindseye compile] && %compile_mindseye_shaders% && %compile_mindseye% %root%\mindseye\me_unity.cpp -o mindseye.dll
if %ERRORLEVEL% NEQ 0 (echo [mindseye compile] Error:%ERRORLEVEL% && exit /b)

if "%testbed%"=="1" echo [testbed compile] && %compile_testbed% %root%\projects\testbed\testbed.cpp -o testbed.dll
if %ERRORLEVEL% NEQ 0 (echo [testbed compile] Error:%ERRORLEVEL% && exit /b)

if "%driver%"=="1" echo [driver compile] && %compile_driver% %root%\mindseye\platform\driver.cpp -o driver.exe
if %ERRORLEVEL% NEQ 0 (echo [driver compile] Error:%ERRORLEVEL% && exit /b)

echo Successfully built. See %root%\build

if "%run%"=="1" echo Running... && call testbed.exe

popd


popd
endlocal