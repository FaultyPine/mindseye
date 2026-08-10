#include "renderer_frontend.h"

#include "core/me_core.h"

#include "bgfx_backend/bgfx_backend.h"
#include "nvrhi_backend/nvrhi_vulkan_backend.h"

void RendererInitialize(EngineContext* engine)
{
    if constexpr (RENDERER_BACKEND == BGFX)
    {
        engine->renderer = MENEW(&engine->engineArena, BgfxRendererBackend);
    }
    else if constexpr (RENDERER_BACKEND == NVRHI_VULKAN)
    {
        engine->renderer = MENEW(&engine->engineArena, NvrhiVulkanRendererBackend);
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
