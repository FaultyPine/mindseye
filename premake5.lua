-- premake5.lua
require "ninja"
workspace "mindseye"
   configurations { "Debug", "Release" }
   platforms { "Win64" }

   includedirs({"."})
   include "mindseye/premake5.lua"
   include "projects/premake5.lua"