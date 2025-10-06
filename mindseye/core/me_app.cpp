

#include "me_core.h"
#include "core/me_log.h"
#include "core/me_memory.h"
#include "core/me_cmdline.h"

#include "platform/me_os.h"
#include "render/renderer_frontend.h"
#include "asset/me_asset.h"
#include "core/thread/me_thread.h"

#include "render/me_material.h"
#include "render/me_texture.h"

EngineContext* GetEngineCtx()
{
    static EngineContext eng;
    return &eng;
}

void InternalRegisterAppCallbacks(AppCallbacks callbacks)
{
    EngineContext* engine = GetEngineCtx();
    engine->callbacks = callbacks;
}

void RunEngine(EngineContext* engine)
{
    while (engine->isRunning)
    {
        meOSTick(engine);
		engine->sceneSystem->Tick(engine);
        RenderInput renderInput = {};
        renderInput.osData = *engine->osData;
		engine->sceneSystem->CopyToRenderInput(renderInput.scene);
        void* renderedSceneHandle = engine->renderer->RenderScene(&renderInput);
        UNUSED(renderedSceneHandle);
		GetTLScratch()->meClear(); // clear the main engine thread's scratch buffer every frame
    }
    engine->renderer->Teardown(engine);
}

void InitializeEngine(s32 argc, char** argv)
{
	meThreadSetName("Engine Main Thread");
    EngineContext* engine = GetEngineCtx();
    engine->isRunning = true;

    InitializeLogger();
    InitializeAllocatorSystem(engine);
    InitializeCmdLine(argc, argv);
    meAssetInitialize(engine);     
	meSceneInitialize(engine);
	meMaterialInitialize(engine);
	meTextureInitialize(engine);
    
	WindowCreationParams windowCreationParams = {}; // TODO: from config/cmdline?
    meOSCreateWindow(windowCreationParams, engine);
    RendererInitialize(engine);

    engine->callbacks.initFn(engine);
    RunEngine(engine);
}
