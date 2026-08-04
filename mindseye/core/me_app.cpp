

#include "me_core.h"
#include "core/me_log.h"
#include "core/me_memory.h"
#include "core/me_cmdline.h"

#include "platform/me_os.h"
#include "render/renderer_frontend.h"
#include "asset/me_asset.h"
#include "asset/me_asset_index.h"
#include "core/thread/me_thread.h"
#include "editor/me_editor.h"

#include "render/me_material.h"
#include "render/me_texture.h"
#include "render/me_mesh.h"
#include "render/me_shader.h"
#include "scene/me_entity.h"
#include "core/me_serialize.h"
#include "core/me_chunker.h"
#include "core/containers/me_blocklist.h"
#include "tests/me_descriptor_tests.h"
#include "core/me_command.h"

#include "generatedtypes/me_app.generated.h"

static EngineContext g_eng;
EngineContext* GetEngineCtx()
{
    return &g_eng;
}

f32 GetDeltaTime()
{
	return GetEngineCtx()->deltaTime;
}

s32 meGetRandom(s32 start, s32 end)
{
	UNIMPLEMENTED();
}

f32 meGetRandomf(f32 start, f32 end)
{
	UNIMPLEMENTED();
}

void InternalRegisterApp(MindseyeAppCallbacks appCallbacks)
{
    EngineContext* engine = GetEngineCtx();
    engine->userApp.slots[engine->userApp.pendingSlot].callbacks = appCallbacks;
}

void CopyToRenderInput(
	EngineContext* engine,
	RenderInput& renderInput)
{
	renderInput.osData = *engine->osData;
	engine->sceneSystem->CopyToRenderInput(renderInput.scene, &engine->engineFrameAllocator);
	renderInput.editorCtx = *engine->editor;
}

// runs on the main thread — loads new dll, swaps slot, unloads old
static void DoUserAppDllSwap(void*)
{
    EngineContext* engine = GetEngineCtx();
    HotReloadData& data = engine->hotReload;
    meLoadedUserApp& app = engine->userApp;

    app.pendingSlot = data.newSlot;
    void* newHandle = LoadDynamicLibrary(data.newDllPath);
    app.pendingSlot = 0;

    if (!newHandle)
    {
        LOG_ERROR("[HotReload] Failed to load %s", data.newDllPath);
        return;
    }

    UnloadDynamicLibrary(app.slots[app.activeSlot].dllHandle);
    app.slots[app.activeSlot] = {};

    app.slots[data.newSlot].dllHandle = newHandle;
    app.dllCounter = data.newCounter;
    app.activeSlot = data.newSlot;

    LOG_INFO("[HotReload] Reloaded -> %s", data.newDllPath);
}

// fires on the process watcher thread when the build exits
static void OnHotReloadBuildComplete(s32 exitCode, void*)
{
    HotReloadData& data = GetEngineCtx()->hotReload;

    // 0 = rebuilt, 2 = nothing changed, 1 = error
    if (exitCode == 2) { LOG_INFO("[HotReload] No changes."); return; }
    if (exitCode != 0) { LOG_ERROR("[HotReload] Build failed (%d).", exitCode); return; }

    if (!meOSCopyFile(data.builtDllPath, data.newDllPath))
    {
        LOG_ERROR("[HotReload] Failed to copy dll to %s", data.newDllPath);
        return;
    }

    meExternalCommand cmd = {};
    cmd.type = meExternalCommandType_MainThreadCmd;
    cmd.mainThreadCmd = { DoUserAppDllSwap, nullptr };
    meSendExternalCommand(cmd);
}

void CheckForUserAppReload(EngineContext* engine)
{
    // VK_F5
    if (!engine->osData->keyboardState.IsKeyJustPressed(0x74))
        return;

    StringView exeFolder = meOSGetExeFileFolder();
    String buildBatPath  = StringFormatTmp("%.*s\\..\\build.bat", STRING_VAARGS(exeFolder));
    #ifdef OS_WINDOWS
    String command = StringFormatTmp("cmd.exe /C \"%s\" app-only", buildBatPath.cstr());
    #else
    LOG_ERROR("Hot reload not supported on this platform");
    return;
    #endif

    meLoadedUserApp& app = engine->userApp;
    HotReloadData& data  = engine->hotReload;
    data.newCounter = app.dllCounter + 1;
    data.newSlot    = 1 - app.activeSlot;
    snprintf(data.builtDllPath, ME_PATH_MAX, "%.*s/%.*s.dll",
        (int)exeFolder.len, exeFolder.data, STRING_VAARGS(engine->appConfig.appName));
    snprintf(data.newDllPath, ME_PATH_MAX, "%.*s/%.*s-%d.dll",
        (int)exeFolder.len, exeFolder.data, STRING_VAARGS(engine->appConfig.appName), data.newCounter);

    LOG_INFO("[HotReload] Building...");
    meOSRunProcessAsync(nullptr, command, OnHotReloadBuildComplete, nullptr);
}

void RunEngine(EngineContext* engine)
{
    while (engine->isRunning)
    {
		f32 time = GetTimeSec();
		engine->deltaTime = time - engine->lastFrameTime;
		engine->renderer->BeginImguiContext();
        meOSTick(engine);
        meFlushMainThreadCommands();
        CheckForUserAppReload(engine);
		engine->userApp.ActiveCallbacks().updateFn(engine);
		engine->sceneSystem->Tick(engine);
		meEditorTick(engine);
        RenderInput renderInput = {};
		// each frame, "snapshot" all the data the renderer will need to render a given frame
		CopyToRenderInput(engine, renderInput);
        void* renderedSceneHandle = engine->renderer->RenderScene(&renderInput);
        UNUSED(renderedSceneHandle);
		engine->renderer->EndImguiContext();
		engine->engineFrameAllocator.meClear();
		GetTLScratch()->meClear(); // clear the main engine thread's scratch buffer every frame
		engine->lastFrameTime = time;
		engine->frameCount++;
    }
    engine->renderer->Teardown(engine);
}

static void InitializeEngineConfig(EngineContext* engine)
{
	// start doing a scan from cwd
	StringView mindseyeIniFile = STRING_LIT("mindseye.json");
	StringView userProjectConfigPath = meFsScanOutForFile(mindseyeIniFile);
	if (!userProjectConfigPath)
	{
		// couldn't find mindseye.ini from cwd, try from exe location
		StringView exeFolder = meOSGetExeFileFolder();
		StringView userConfigExeFolder = StringFormatTmp("%.*s%.*s%.*s", STRING_VAARGS(exeFolder), STRING_VAARGS(meFsGetDirectorySeperator()), STRING_VAARGS(mindseyeIniFile));
		userProjectConfigPath = meFsScanOutForFile(userConfigExeFolder);
	}
	if (!userProjectConfigPath)
	{
		LOG_ERROR("Couldn't find mindseye.ini.");
	}
	else
	{
		meSerializeResult result = {};
		DeserializeContext deserializeCtx = {};
		deserializeCtx.mode = meSerializationMode_Text;
		deserializeCtx.typeDesc = &TD_MEUSERCONFIG;
		deserializeCtx.externalDataAllocator = &engine->engineArena;
		deserializeCtx.outputData = SPAN_FROM(engine->userConfig);
		deserializeCtx.outResult = &result;
		DeserializeFromFileBlocking(userProjectConfigPath, deserializeCtx);
        if (result.result != meSerializeResult::ResultType::SER_SUCCESS)
        {
            LOG_WARN("failed to deserialize user config!");
            return;
        }
        // "userApp" referring to a program that uses the mindseye engine
		StringView userAppConfigFile = engine->userConfig.projectRootConfigFile;
		String userAppConfigPathAbs = meOSResolveRelativeToAbsPath(GetTLScratch(), userAppConfigFile);
        result = {};
		deserializeCtx = {};
		deserializeCtx.mode = meSerializationMode_Text;
		deserializeCtx.typeDesc = &TD_MEAPPCONFIG;
		deserializeCtx.externalDataAllocator = &engine->engineArena;
		deserializeCtx.outputData = SPAN_FROM(engine->appConfig);
		deserializeCtx.outResult = &result;
		DeserializeFromFileBlocking(userAppConfigPathAbs, deserializeCtx);
        if (result.result != meSerializeResult::ResultType::SER_SUCCESS)
        {
            LOG_WARN("failed to deserialize app config!");
            return;
        }

        StringView exeFolder = meOSGetExeFileFolder();
        String userAppDllPath = StringFormatTmp("%.*s/%.*s-1.dll",
            STRING_VAARGS(exeFolder), STRING_VAARGS(engine->appConfig.appName));
        engine->userApp.pendingSlot = 0;
		void* gameLib = LoadDynamicLibrary(userAppDllPath.cstr());
		if (!gameLib)
		{
			LOG_ERROR("Failed to load game library %.*s", STRING_VAARGS(userAppDllPath));
            return;
		}
        engine->userApp.slots[0].dllHandle = gameLib;
        engine->userApp.activeSlot = 0;
        engine->userApp.dllCounter = 1;
	}
}

void InitializeEngineSystems(EngineContext* engine)
{
    meInitMainThreadCommandQueue();
	InitializeEngineConfig(engine);
	RendererInitialize(engine);
	meAssetInitialize(engine);
	meSceneInitialize(engine);
	meMaterialInitialize(engine);
	meTextureInitialize(engine);
	meMeshInitialize(engine);
	meShaderInitialize(engine);
	InitializeEntitySystem(engine);

	meAssetInitializeLate(engine);
	meAssetIndexInitialize(engine);

	meSceneInitializeLate(engine);

}

void RunEngineTests(EngineContext* engine)
{
    meRelPtrTests();
    meChunkerTests();
    DynArrayTests();
    HybridArrayTests();
    TestBlocklist();
    meDescriptorTests();
}

void DeinitializeEngineSystems(EngineContext* engine)
{
    DeinitializeEntitySystem(engine);
    meShutdownMainThreadCommandQueue();
    meOSFreeVirtualMemory(engine->rootArena.backing_mem);
}

void InitializeEngine(s32 argc, char** argv)
{
	meThreadSetName("Engine Main Thread");
    EngineContext* engine = GetEngineCtx();
    engine->isRunning = true;

    InitializeAllocatorSystem(engine);
    InitializeCmdLine(argc, argv);
    meCrashHandlerContext crashHandlerContext = {};
    crashHandlerContext.isRunningTests = CMDLINE_HAS(ShouldRunTests);
    meOSInstallUnhandledExceptionHandler(crashHandlerContext);

    InitializeLogger();
	StringView workingDir = meOSGetWorkingDir();
	LOG_INFO("Working dir: %.*s", STRING_VAARGS(workingDir));
	    
	WindowCreationParams windowCreationParams = {}; // TODO: from config/cmdline?
    meOSCreateWindow(windowCreationParams, engine);

	InitializeEngineSystems(engine);

	// TODO: In the future, this'll be a proper toggle of some sort that gets compiled out of shipping builds
	bool editorEnabled = true;
	if (editorEnabled)
	{
		meEditorInitialize(engine);
		meEditorGetCtx().editorCamera.isControlledByUserInput = true;
	}

    if (CMDLINE_HAS(ShouldRunTests))
    {
    RunEngineTests(engine);
        return;
    }

	meOSSetCursorState(CAPTURED, *engine->osData);
	
    engine->userApp.ActiveCallbacks().initFn(engine);

	// default scene load
	if (engine->appConfig.defaultSceneName)
	{
        engine->sceneSystem->ChangeCurrentSceneAsync(engine->appConfig.defaultSceneName);
	}

    RunEngine(engine);
	engine->userApp.ActiveCallbacks().shutdownFn(engine);
    DeinitializeEngineSystems(engine);
}
