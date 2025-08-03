#pragma once

#include "core/me_defines.h"
#include "core/me_arena.h"
#include "platform/me_os.h" 

struct RendererFrontend;

struct EngineContext;
typedef void(*InitFn)(EngineContext* engine, WindowCreationParams& windowCreationParams);
typedef void(*UpdateFn)(EngineContext* engine);
typedef void(*ShutdownFn)(EngineContext* engine);
struct AppCallbacks
{
    InitFn initFn = nullptr;
    UpdateFn updateFn = nullptr;
    ShutdownFn shutdownFn = nullptr;
};
struct EngineContext
{
    AppCallbacks callbacks = {};
    // allocators
    Arena gameArena = {};
    Arena engineArena = {}; // persistent, never cleared
    Arena engineFrameAllocator = {}; // cleared at the end of each frame
    Arena engineSceneAllocator = {}; // persistent for a scene
    Arena scratchWork = {}; // individual systems are in charge of handling their own allocations here.
    
    // systems
    RendererFrontend* renderer = nullptr;

    // engine state
    f32 deltaTime = 0.0f;
    f32 lastFrameTime = 0.0f;
    u32 frameCount = 0;
    u64 randomSeed = 0;

    u32 windowWidth = 0;
    u32 windowHeight = 0; 
    // u32 aspectRatioW = 0; 
    // u32 aspectRatioH = 0;
    String appName = {};
    OSCookbook* osData = {};

    bool isRunning = false;
    bool isIdle = false;
};
MEAPI EngineContext* GetEngineCtx();

// register a program
MEAPI void InternalRegisterAppCallbacks(AppCallbacks callbacks);
#define REGISTER_ME_CALLBACKS(appCallbacks) \
    struct ME_CALLBACKS_STRUCT { \
        ME_CALLBACKS_STRUCT() { InternalRegisterAppCallbacks(appCallbacks); } \
    }; \
    static ME_CALLBACKS_STRUCT globalCallbacksHolder = {};


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


MEAPI void InitializeEngine();
