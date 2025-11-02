#pragma once

#include "mindseye/render/renderer_frontend.h"

struct BgfxRendererBackend : public RendererFrontend
{
    void Initialize(EngineContext* engine) override;
    void Teardown(EngineContext* engine) override;
    void* RenderScene(RenderInput* scene) override;

	// each renderer backend is responsible for drawing the ImDrawData imgui produces
	virtual void BeginImguiContext() override;
	virtual void EndImguiContext() override;

	virtual u64 CreateShaderProgram(meSpan fsMem, meSpan vsMem) override;
	virtual u64 UploadTextureToGPU(meSpan textureMem, u32 channels, u32 width, u32 height) override;
	virtual void LoadSceneRuntime(meScene& scene, meAllocator* allocator) override;
};