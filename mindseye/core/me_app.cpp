

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
    engine->appCallbacks = appCallbacks;
}

void CopyToRenderInput(
	EngineContext* engine,
	RenderInput& renderInput)
{
	renderInput.osData = *engine->osData;
	engine->sceneSystem->CopyToRenderInput(renderInput.scene);
	renderInput.editorCtx = *engine->editor;
}

void RunEngine(EngineContext* engine)
{
    while (engine->isRunning)
    {
		f32 time = GetTimeSec();
		engine->deltaTime = time - engine->lastFrameTime;
		engine->renderer->BeginImguiContext();
        meOSTick(engine);
		engine->appCallbacks.updateFn(engine);
		engine->sceneSystem->Tick(engine);
		meEditorTick(engine);
        RenderInput renderInput = {};
		CopyToRenderInput(engine, renderInput);
        void* renderedSceneHandle = engine->renderer->RenderScene(&renderInput);
        UNUSED(renderedSceneHandle);
		engine->renderer->EndImguiContext();
		GetTLScratch()->meClear(); // clear the main engine thread's scratch buffer every frame
		engine->lastFrameTime = time;
    }
    engine->renderer->Teardown(engine);
}

static void InitializeEngineConfig(EngineContext* engine)
{
	// start doing a scan from cwd
	StringView mindseyeIniFile = STRING_LIT("mindseye.ini");
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
		SerializeFromFile(userProjectConfigPath, &engine->engineArena, TD_MEUSERCONFIG, meSpan(&engine->userConfig, sizeof(engine->userConfig)));
		// "userApp" referring to a program that uses the mindseye engine
		StringView userAppConfigFile = engine->userConfig.projectRootConfigFile;
		String userAppConfigPathAbs = meOSResolveRelativeToAbsPath(GetTLScratch(), userAppConfigFile);
		SerializeFromFile(userAppConfigPathAbs, &engine->engineArena, TD_MEAPPCONFIG, meSpan(&engine->appConfig, sizeof(engine->appConfig)));

		StringView userAppDllName = StringFormatTmp("%.*s.dll", STRING_VAARGS(engine->appConfig.appName));
		void* gameLib = LoadDynamicLibrary(userAppDllName.cstr());
		if (!gameLib)
		{
			LOG_ERROR("Failed to load game library %.*s", STRING_VAARGS(userAppDllName));
		}
	}
}

void InitializeEngineSystems(EngineContext* engine)
{
	InitializeEngineConfig(engine);
	RendererInitialize(engine);
	meAssetInitialize(engine);
	meSceneInitialize(engine);
	meMaterialInitialize(engine);
	meTextureInitialize(engine);
	meMeshInitialize(engine);
	meShaderInitialize(engine);
	Entity::InitializeEntitySystem(&engine->engineSceneAllocator);
	meAssetIndexInitialize(engine);
}

void InitializeEngine(s32 argc, char** argv)
{
	meThreadSetName("Engine Main Thread");
    EngineContext* engine = GetEngineCtx();
    engine->isRunning = true;

    InitializeLogger();
	StringView workingDir = meOSGetWorkingDir();
	LOG_INFO("Working dir: %.*s", STRING_VAARGS(workingDir));
    InitializeAllocatorSystem(engine);
    InitializeCmdLine(argc, argv);
	    
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

	meOSSetCursorState(CAPTURED, *engine->osData);

	// default scene load
	meAssetIdent sceneIdent = meAssetGetIdentFromPath(engine->appConfig.defaultSceneName);
	if (sceneIdent)
	{
		auto onSceneLoad = +[](const meAsset& asset)
		{
			meScene& loadedSceneData = meScenePoolGet().Get(asset.runtimeHandle);
			GetEngineCtx()->sceneSystem->CurrentScene() = loadedSceneData;
			GetEngineCtx()->appCallbacks.onSceneLoadFn(GetEngineCtx());
		};
		meAssetRequestLoad(&sceneIdent, 1, onSceneLoad);
	}
	
    engine->appCallbacks.initFn(engine);
    RunEngine(engine);
	engine->appCallbacks.shutdownFn(engine);
}
