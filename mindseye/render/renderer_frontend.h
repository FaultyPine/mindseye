#pragma once

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

    virtual void Initialize() {}
};

void RendererInitialize(EngineContext* engine);
void RendererTeardown();
