#include "renderer_frontend.h"

#include "core/me_core.h"

#include "bgfx_backend/bgfx_backend.h"

void RendererInitialize(EngineContext* engine)
{
    if constexpr (RENDERER_BACKEND == BGFX)
    {
        engine->renderer = MENEW(&engine->engineArena, BgfxRendererBackend);
    }
    engine->renderer->Initialize(engine);
}

void RendererTeardown()
{

}

RendererFrontend& RendererGetMain()
{
	return *GetEngineCtx()->renderer;
}