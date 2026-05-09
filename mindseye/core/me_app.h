#pragma once

#include "core/me_defines.h"
#include "core/me_arena.h"
#include "core/me_event.h"

#include "platform/me_os.h" 
#include "platform/me_input.h"

struct RendererFrontend;
struct EngineContext;
struct meAssetSystem;
struct CommandLineArgs;
struct meSceneManager;
struct meMaterialPool;
struct meTexturePool;
struct meMeshPool;
struct meShaderPool;
struct meScenePool;
struct meEntityPool;
struct EditorContext;
struct meAssetIndex;

struct MEREFLECT(type) meUserConfig
{
	String projectRootConfigFile = {};
};
struct MEREFLECT(type) meAppConfig
{
	String appName = {};
	String resourcesDir = {};
	String defaultSceneName = {};
    u32 pixelsPerUnit = 100;
};

typedef void(*InitFn)(EngineContext* engine);
typedef void(*UpdateFn)(EngineContext* engine);
typedef void(*ShutdownFn)(EngineContext* engine);

inline void defaultInitFn(EngineContext *){}
inline void defaultUpdateFn(EngineContext *){}
inline void defaultShutdownFn(EngineContext *){}

struct MindseyeAppCallbacks
{
    InitFn initFn = defaultInitFn;
    UpdateFn updateFn = defaultUpdateFn;
    ShutdownFn shutdownFn = defaultShutdownFn;
    meEvent onSceneLoaded;
};

struct AppDllSlot
{
    MindseyeAppCallbacks callbacks = {};
    void* dllHandle = nullptr;
};

struct meLoadedUserApp
{
    AppDllSlot slots[2] = {};
    s32 activeSlot  = 0;
    s32 dllCounter  = 1; // matches the -N suffix of the currently loaded dll
    s32 pendingSlot = 0; // slot InternalRegisterApp writes into during LoadDynamicLibrary

    MindseyeAppCallbacks& ActiveCallbacks() { return slots[activeSlot].callbacks; }
};

struct EngineContext
{
    meLoadedUserApp userApp = {};

	StringView appRootConfig = STRING_LIT(".");
    // allocators
    Arena gameArena = {};
    Arena engineArena = {}; // persistent, never cleared
    Arena engineFrameAllocator = {}; // cleared at the end of each frame
    Arena engineSceneAllocator = {}; // persistent for a scene
    
    RendererFrontend* renderer = nullptr;
	EditorContext* editor = nullptr;
	meAssetIndex* assetIndex = nullptr;
    meAssetSystem* assetSystem = nullptr;
    CommandLineArgs* cmdLine = nullptr;
	
	meScenePool* scenePool = nullptr;
    meEntityPool* entityPool = nullptr;
	meMaterialPool* materialSystem = nullptr;
	meTexturePool* textureSystem = nullptr;
	meMeshPool* meshSystem = nullptr;
	meShaderPool* shaderSystem = nullptr;
	
	meSceneManager* sceneSystem = nullptr;
	

    // engine state
    f32 deltaTime = 0.0f;
    f32 lastFrameTime = 0.0f;
    u32 frameCount = 0;
    u64 randomSeed = 0;
    
	meUserConfig userConfig = {};
	meAppConfig appConfig = {};
    String appName = {};
    
    // NOTE: points to static data. This is so we don't include the OSState in record/replay stuff
    OSStateView* osData = nullptr;

    bool isRunning = false;
    bool isIdle = false;
};
MEAPI EngineContext* GetEngineCtx();
MEAPI f32 GetDeltaTime();
MEAPI s32 meGetRandom(s32 start, s32 end);
MEAPI f32 meGetRandomf(f32 start, f32 end);


MEAPI void InternalRegisterApp(MindseyeAppCallbacks callbacks);
#define REGISTER_MINDSEYE_APP(...) \
    struct ME_APPREG_STRUCT { \
	ME_APPREG_STRUCT() { InternalRegisterApp(MindseyeAppCallbacks(__VA_ARGS__)); } \
    }; \
    static ME_APPREG_STRUCT globalAppRegistrationHolder = {};

MEAPI void CheckForUserAppReload(EngineContext* engine);

MEAPI void InitializeEngine(s32 argc, char** argv);
