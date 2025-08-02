#pragma once

#include "mindseye/render/renderer_frontend.h"

struct BgfxRendererBackend : public RendererFrontend
{
    void Initialize() override;
};