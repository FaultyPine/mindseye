@echo off


mkdir "mindseye/external/vulkan_lib/Lib"
powershell -command "Invoke-WebRequest -Uri https://sdk.lunarg.com/sdk/download/1.4.321.0/windows/VulkanRT-X64-1.4.321.0-Components.zip -OutFile mindseye/external/vulkan_lib/Lib/vulkan.zip"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)
@REM start /wait /b powershell -command "Expand-Archive tools/clang.zip tools/clang_inner"
tar -zxvf "mindseye/external/vulkan_lib/Lib/vulkan.zip" -C "mindseye/external/vulkan_lib/Lib"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)
del "mindseye\external\vulkan_lib\Lib\vulkan.zip"

move /Y "mindseye\external\vulkan_lib\Lib\VulkanRT-X64-1.4.321.0-Components\x64\*.*" "mindseye\external\vulkan_lib\Lib"
rmdir /S /Q "mindseye\external\vulkan_lib\Lib\VulkanRT-X64-1.4.321.0-Components"
