#pragma once

#include "core/me_defines.h"
#include "core/me_arena.h"
#include "core/me_event.h"

#include "platform/me_os.h" 
#include "platform/me_input.h"

struct RendererFrontend;
struct EngineContext;
struct EntityRegistry;
struct meAssetSystem;
struct CommandLineArgs;
struct meSceneManager;
struct meMaterialPool;
struct meTexturePool;
struct meMeshPool;
struct meShaderPool;

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

struct AppRegistrationInfo
{
    InitFn initFn = defaultInitFn;
    UpdateFn updateFn = defaultUpdateFn;
    ShutdownFn shutdownFn = defaultShutdownFn;
};

struct EngineContext
{
    AppRegistrationInfo appInfo = {};
	StringView appRootConfig = STRING_LIT(".");
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
	meMeshPool* meshSystem = nullptr;
	meShaderPool* shaderSystem = nullptr;

    // engine state
    f32 deltaTime = 0.0f;
    f32 lastFrameTime = 0.0f;
    u32 frameCount = 0;
    u64 randomSeed = 0;
    
	meUserConfig userConfig = {};
	meAppConfig appConfig = {};
    String appName = {};
    OSStateView* osData = nullptr; // static, persistent throughout app

    bool isRunning = false;
    bool isIdle = false;
};
MEAPI EngineContext* GetEngineCtx();
MEAPI f32 GetDeltaTime();

// register a program
MEAPI void InternalRegisterApp(AppRegistrationInfo callbacks);
// pass parameters to AppRegistrationInfo constructor
#define REGISTER_MINDSEYE_APP(...) \
    struct ME_APPREG_STRUCT { \
	ME_APPREG_STRUCT() { InternalRegisterApp(AppRegistrationInfo(__VA_ARGS__)); } \
    }; \
    static ME_APPREG_STRUCT globalAppRegistrationHolder = {};


MEAPI void InitializeEngine(s32 argc, char** argv);
