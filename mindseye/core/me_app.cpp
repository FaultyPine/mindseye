

#include "me_core.h"
#include "core/me_log.h"
#include "core/me_memory.h"
#include "core/me_cmdline.h"

#include "platform/me_os.h"
#include "render/renderer_frontend.h"
#include "asset/me_asset.h"
#include "core/thread/me_thread.h"

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
        RenderInput renderInput = {};
        renderInput.osData = *engine->osData;
        void* renderedSceneHandle = engine->renderer->RenderScene(&renderInput);
        UNUSED(renderedSceneHandle);
    }
    engine->renderer->Teardown(engine);
}

void InitializeEngine(s32 argc, char** argv)
{
	meThreadSetName("Engine Main Thread");
    EngineContext* engine = GetEngineCtx();
    engine->isRunning = true;
    engine->engineInitialize();
    InitializeLogger(engine);
    InitializeAllocatorSystem(engine);
    InitializeCmdLine(argc, argv);
    meAssetInitialize(engine);
    WindowCreationParams windowCreationParams = {};
    engine->callbacks.initFn(engine, windowCreationParams);
    meOSCreateWindow(windowCreationParams, engine);
    RendererInitialize(engine);

    RunEngine(engine);
}
