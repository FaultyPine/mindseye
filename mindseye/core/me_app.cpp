

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
#include "core/me_serialize.h"

#include "generatedtypes/me_app.generated.cpp"

static EngineContext g_eng;
EngineContext* GetEngineCtx()
{
    return &g_eng;
}

f32 GetDeltaTime()
{
	return GetEngineCtx()->deltaTime;
}

void InternalRegisterApp(MindseyeAppCallbacks appCallbacks)
{
    EngineContext* engine = GetEngineCtx();
    engine->appCallbacks = appCallbacks;
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

	InitializeEngineSystems(engine);
	meEditorInitialize(engine);

	meOSSetCursorState(CAPTURED, *engine->osData);

	// start doing a scan from cwd
	StringView mindseyeIniFile = STRING_LIT("mindseye.ini");
	StringView userProjectConfigPath = meFsScanOutForFile(mindseyeIniFile);
	if (!userProjectConfigPath)
	{
		// couldn't find mindseye.ini from cwd, try from exe location
		StringView exeFolder = meOSGetExeFileFolder();
		StringView userConfigExeFolder = StringFormat("%.*s%.*s%.*s", STRING_VAARGS(exeFolder), STRING_VAARGS(meFsGetDirectorySeperator()), STRING_VAARGS(mindseyeIniFile));
		userProjectConfigPath = meFsScanOutForFile(userConfigExeFolder);
	}
	if (!userProjectConfigPath)
	{
		LOG_ERROR("Couldn't find mindseye.ini.");
	}
	else
	{
        {
            OSFileReference file;
            meOSOpenFile(file, userProjectConfigPath, OSFileFlags(OnlyIfExists | ScopedFile));
            ScopedAllocation tmpFileContent(GetTLScratch(), meOSGetFileSize(file));
            meOSReadFileContents(file, tmpFileContent.allocation.data, tmpFileContent.allocation.size);
            DeserializeFromTextBlocking(TD_MEUSERCONFIG, &engine->engineArena, StringView(tmpFileContent.allocation), meSpan(&engine->userConfig, sizeof(engine->userConfig)));
        }
		// "userApp" referring to a program that uses the mindseye engine
		StringView userAppConfigFile = engine->userConfig.projectRootConfigFile;
		String userAppConfigPathAbs = meOSResolveRelativeToAbsPath(GetTLScratch(), userAppConfigFile);
        {
            OSFileReference file;
            meOSOpenFile(file, userAppConfigPathAbs, OSFileFlags(OnlyIfExists | ScopedFile));
            ScopedAllocation tmpFileContent(GetTLScratch(), meOSGetFileSize(file));
            meOSReadFileContents(file, tmpFileContent.allocation.data, tmpFileContent.allocation.size);
            DeserializeFromTextBlocking(TD_MEAPPCONFIG, &engine->engineArena, StringView(tmpFileContent.allocation), meSpan(&engine->appConfig, sizeof(engine->appConfig)));
        }
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
            meAssetIdent sceneIdent = meAssetIdent(engine->appConfig.defaultSceneName, meAssetType::Scene);
            auto onSceneLoad = +[](const meRTAsset& asset)
            {
                meScene* loadedSceneData = (meScene*)asset.loadedData.data;
                meSceneManager::CurrentScene() = *loadedSceneData;
                GetEngineCtx()->appCallbacks.onSceneLoadFn(GetEngineCtx());
            };
            meAssetRequestLoad(GetTLScratch(), &sceneIdent, 1, onSceneLoad);
		}
	}

    engine->appCallbacks.initFn(engine);
    RunEngine(engine);
	engine->appCallbacks.shutdownFn(engine);
}
