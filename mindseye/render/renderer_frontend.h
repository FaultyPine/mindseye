#pragma once

struct EngineContext;

enum RendererBackendType
{
    VULKAN,
};

struct Renderer
{
    RendererBackendType backendType = VULKAN;
    Arena rendererArena = {};
};

void RendererInitialize(EngineContext* engine);
void RendererTeardown();
