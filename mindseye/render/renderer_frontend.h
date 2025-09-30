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

// after creation, intended as a readonly container
// of everything the renderer needs to render a frame.
struct RenderInput
{
    OSStateView osData; // window width/height, mouse state, etc
    meScene scene;
};

struct RendererFrontend
{
    RendererBackendType backendType = NONE;
    meAllocator* rendererPersistentAllocator = nullptr;
    Arena rendererFrameArena = {};
    bool rendererLoggingEnabled = true;

    virtual void Initialize(EngineContext* engine) {}
    virtual void Teardown(EngineContext* engine) {}

    // takes in all the input the renderer needs to render a frame. Outputs a framebuffer (handle)
    virtual void* RenderScene(RenderInput* input) { return nullptr; }
};

void RendererInitialize(EngineContext* engine);
void RendererTeardown();
