#pragma once

#include "mindseye/render/renderer_frontend.h"

struct BgfxRendererBackend : public RendererFrontend
{
    void Initialize(EngineContext* engine) override;
    void Teardown(EngineContext* engine) override;
    void* RenderScene(meScene* scene) override;
};