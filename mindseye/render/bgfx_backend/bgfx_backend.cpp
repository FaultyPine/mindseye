
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
    bgfx::touch(0);
    bgfx::dbgTextPrintf(0, 1, 0x0f, "Color can be changed with ANSI \x1b[9;me\x1b[10;ms\x1b[11;mc\x1b[12;ma\x1b[13;mp\x1b[14;me\x1b[0m code too.");
    bgfx::frame();
    return nullptr;
}