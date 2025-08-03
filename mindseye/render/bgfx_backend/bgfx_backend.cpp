
#include "bgfx_backend.h"

#include "external/bgfx/bgfx/include/bgfx/bgfx.h"

#include "core/me_core.h"

void BgfxRendererBackend::Initialize(EngineContext* engine)
{
    bgfx::Init init;
    init.type = bgfx::RendererType::Vulkan;
    init.vendorId = BGFX_PCI_ID_NONE; // prioritize integrated? discrete? microsft/nvidia/amd adapter? None means do it automatically
    init.platformData.ndt = nullptr;
    init.platformData.nwh = engine->osData->hwnd; // bgfx renderer backend needs platform window handle, this is hardcoded to windows rn. If another platform is supported in the future, this'll throw a compiler error
    init.resolution.width = engine->windowWidth;
    init.resolution.height = engine->windowHeight;
    init.resolution.reset = BGFX_RESET_VSYNC;
    bgfx::init(init);

    bgfx::setDebug(BGFX_DEBUG_TEXT);

    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

}


void BgfxRendererBackend::Teardown(EngineContext* engine)
{
    bgfx::shutdown();
}

void* BgfxRendererBackend::RenderScene(meScene* scene)
{

    return nullptr;
}