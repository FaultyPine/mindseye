#pragma once

#include "core/me_defines.h"
#include "core/me_arena.h"
#include "core/me_event.h"

#include "platform/me_os.h" 

struct RendererFrontend;
struct EngineContext;
struct EntityRegistry;
struct meAssetSystem;
struct CommandLineArgs;
struct meSceneManager;
struct meMaterialPool;
struct meTexturePool;

typedef void(*InitFn)(EngineContext* engine);
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
    
    // systems
    RendererFrontend* renderer = nullptr;
    EntityRegistry* entityRegistry = nullptr;
    meAssetSystem* assetSystem = nullptr;
    CommandLineArgs* cmdLine = nullptr;
	meSceneManager* sceneSystem = nullptr;
	meMaterialPool* materialSystem = nullptr;
	meTexturePool* textureSystem = nullptr;

    // engine state
    f32 deltaTime = 0.0f;
    f32 lastFrameTime = 0.0f;
    u32 frameCount = 0;
    u64 randomSeed = 0;
    
    String appName = {};
    OSStateView* osData = nullptr; // static, persistent throughout app

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


MEAPI void InitializeEngine(s32 argc, char** argv);
