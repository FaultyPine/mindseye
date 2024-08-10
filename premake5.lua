-- premake5.lua
workspace "mindseye"
   configurations { "Debug", "Release" }
   platforms { "Win64" }

   include "mindseye/premake5.lua"
   include "projects/premake5.lua"