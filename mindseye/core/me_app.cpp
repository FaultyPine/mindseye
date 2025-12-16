

#include "me_core.h"
#include "core/me_log.h"
#include "core/me_memory.h"
#include "core/me_cmdline.h"

#include "platform/me_os.h"
#include "render/renderer_frontend.h"
#include "asset/me_asset.h"
#include "core/thread/me_thread.h"
#include "editor/me_editor.h"

#include "render/me_material.h"
#include "render/me_texture.h"
#include "render/me_mesh.h"
#include "render/me_shader.h"
#include "scene/me_entity.h"

#include "generatedtypes/me_app.generated.cpp"

EngineContext* GetEngineCtx()
{
    static EngineContext eng;
    return &eng;
}

f32 GetDeltaTime()
{
	return GetEngineCtx()->deltaTime;
}

void InternalRegisterApp(AppRegistrationInfo appInfo)
{
    EngineContext* engine = GetEngineCtx();
    engine->appInfo = appInfo;
}

void RunEngine(EngineContext* engine)
{
    while (engine->isRunning)
    {
		f32 time = GetTimeSec();
		engine->deltaTime = time - engine->lastFrameTime;
		engine->renderer->BeginImguiContext();
        meOSTick(engine);
		engine->appInfo.updateFn(engine);
		engine->sceneSystem->Tick(engine);
		meEditorDisplayGui(engine);
        RenderInput renderInput = {};
        renderInput.osData = *engine->osData;
		engine->sceneSystem->CopyToRenderInput(renderInput.scene);
        void* renderedSceneHandle = engine->renderer->RenderScene(&renderInput);
        UNUSED(renderedSceneHandle);
		engine->renderer->EndImguiContext();
		GetTLScratch()->meClear(); // clear the main engine thread's scratch buffer every frame
		engine->lastFrameTime = time;
    }
    engine->renderer->Teardown(engine);
}

void InitializeEngineSystems(EngineContext* engine)
{
	RendererInitialize(engine);
	meAssetInitialize(engine);
	meSceneInitialize(engine);
	meMaterialInitialize(engine);
	meTextureInitialize(engine);
	meMeshInitialize(engine);
	meShaderInitialize(engine);
	Entity::InitializeEntitySystem(&engine->engineSceneAllocator);
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
	meEditorInitialize(engine);

	InitializeEngineSystems(engine);

	meOSSetCursorState(CAPTURED, *engine->osData);

	// start doing a scan from cwd
	StringView mindseyeIniFile = STRING_LIT("mindseye.ini");
	StringView userProjectConfigPath = meFsScanOutForFile(mindseyeIniFile, GetTLScratch());
	if (!userProjectConfigPath)
	{
		// couldn't find mindseye.ini from cwd, try from exe location
		StringView exeFolder = meOSGetExeFileFolder();
		StringView userConfigExeFolder = StringFormat("%.*s%.*s%.*s", STRING_VAARGS(exeFolder), STRING_VAARGS(meFsGetDirectorySeperator()), STRING_VAARGS(mindseyeIniFile));
		userProjectConfigPath = meFsScanOutForFile(userConfigExeFolder, GetTLScratch());
	}
	if (!userProjectConfigPath)
	{
		LOG_ERROR("Couldn't find mindseye.ini.");
	}
	else
	{
		DeserializeFromIniBlocking(TD_MEUSERCONFIG, &engine->engineArena, userProjectConfigPath, meSpan(&engine->userConfig, sizeof(engine->userConfig)));
		
		// "userApp" referring to a program that uses the mindseye engine
		StringView userAppConfigFile = engine->userConfig.projectRootConfigFile;
		String userAppConfigPathAbs = meOSResolveRelativeToAbsPath(GetTLScratch(), userAppConfigFile);
		DeserializeFromIniBlocking(TD_MEAPPCONFIG, &engine->engineArena, userAppConfigPathAbs, meSpan(&engine->appConfig, sizeof(engine->appConfig)));
		StringView userAppConfigDir = msFsGetDirFromPath(userAppConfigPathAbs);
		meAssetSetResourceDir(userAppConfigDir);
		
		StringView userAppDllName = StringFormat("%.*s.dll", STRING_VAARGS(engine->appConfig.appName));
		void* gameLib = LoadDynamicLibrary(userAppDllName.cstr());
		if (!gameLib)
		{
			LOG_ERROR("Failed to load game library %.*s", STRING_VAARGS(userAppDllName));
		}

		if (!engine->sceneSystem->CurrentScene().IsValid())
		{
			// if no scene already, and user config specifies a default scene, load it
			engine->sceneSystem->LoadSceneFromFileBlocking(engine->appConfig.defaultSceneName, &engine->engineSceneAllocator, &meSceneManager::CurrentScene());
		}
	}

    engine->appInfo.initFn(engine);
    RunEngine(engine);
	engine->appInfo.shutdownFn(engine);
}
