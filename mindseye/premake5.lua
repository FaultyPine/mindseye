local rootdir = _MAIN_SCRIPT_DIR

project "mindseye"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"
   filter { "platforms:Win64" }
      system "Windows"
      architecture "x64"
   staticruntime "on"
   toolset "clang"
   targetdir(path.join(rootdir, "bin/%{cfg.buildcfg}"))
   objdir(path.join(rootdir, "build/%{cfg.buildcfg}"))
   flags {"MultiProcessorCompile"}
   files { 
      "**.h", 
      "**.cpp" 
   }
   links({

   })
   defines {
      "DLL_EXPORTS",
      "_CRT_SECURE_NO_WARNINGS",
      "MEEXPORT"
   }
   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"