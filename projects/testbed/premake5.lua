local rootdir = _MAIN_SCRIPT_DIR

project "testbed"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"
   filter { "platforms:Win64" }
      system "Windows"
      architecture "x64"
   staticruntime "on"
   toolset "clang"
   targetdir(path.join(rootdir, "bin/%{cfg.buildcfg}"))
   objdir(path.join(rootdir, "build/%{cfg.buildcfg}"))
   includedirs { "external", "mindseye" }
   flags {"MultiProcessorCompile"}
   files { 
      "**.h", 
      "**.cpp" 
   }
   links({
      "mindseye"
   })
   defines {

    
   }
   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"
      
      filter "configurations:Release"
      defines { "NDEBUG" }
      symbols "On"
      optimize "On"
