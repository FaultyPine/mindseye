#pragma once

#include "core/me_defines.h"
#include "core/me_arena.h"

struct EngineContext
{
    // u32 windowWidth = 0;
    // u32 windowHeight = 0; 
    // u32 aspectRatioW = 0; 
    // u32 aspectRatioH = 0;
    // const char* appName = nullptr;

    Arena gameArena = {};
    Arena engineArena = {}; // persistent
    Arena engineFrameAllocator = {}; // cleared at the end of each frame
    Arena engineSceneAllocator = {}; // persistent for a scene

    f32 deltaTime = 0.0f;
    f32 lastFrameTime = 0.0f;
    u32 frameCount = 0;
    //GLFWwindow* glob_glfw_window = nullptr;
    u64 randomSeed = 0;
};

// returns the current time since app launch
MEAPI f64 GetTime();
// just casts GetTime to f32
MEAPI f32 GetTimef();

MEAPI void OverwriteRandomSeed(u64 seed);
MEAPI u64 GetRandomSeed();
MEAPI s32 GetRandom(s32 start, s32 end);
MEAPI f32 GetRandomf(f32 start, f32 end);

MEAPI u32 HashBytes(u8* data, u32 size);
MEAPI u64 HashBytesL(u8* data, u32 size);