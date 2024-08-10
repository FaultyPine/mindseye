
local rootdir = _MAIN_SCRIPT_DIR

project "synod"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++17"
   filter { "platforms:Win64" }
      system "Windows"
      architecture "x64"
   staticruntime "off"
   toolset "clang"
   targetdir(path.join(rootdir, "bin/%{cfg.buildcfg}"))
   objdir(path.join(rootdir, "build/%{cfg.buildcfg}"))
   flags {"MultiProcessorCompile"}
   files { 
      "**.h", 
      "**.cpp" 
   }
   defines {
      
   }
   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"