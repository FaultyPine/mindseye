#pragma once

#include "scene/me_scene.h"
struct EngineContext;


enum RendererBackendType
{
    NONE,
    VULKAN,
    BGFX,
};
#define RENDERER_BACKEND (RendererBackendType::BGFX)

struct RendererFrontend
{
    RendererBackendType backendType = NONE;
    Arena rendererArena = {};

    virtual void Initialize(EngineContext* engine) {}
    virtual void Teardown(EngineContext* engine) {}

    virtual void* RenderScene(meScene* scene) { return nullptr; }
};

void RendererInitialize(EngineContext* engine);
void RendererTeardown();
