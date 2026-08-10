#pragma once

#include "mindseye/render/renderer_frontend.h"

struct NvrhiVulkanState;

struct NvrhiVulkanRendererBackend : public RendererFrontend
{
    void Initialize(EngineContext* engine) override;
    void Teardown(EngineContext* engine) override;
    void* RenderScene(RenderInput* input) override;
    void PresentFrame() override;

	virtual void BeginImguiContext() override;
	virtual void EndImguiContext() override;
    
    u64 CreateVertexBuffer(meSpan bufferMem, meMeshVertexLayoutType layout) override;
    u64 UploadTextureToGPU(meSpan textureMem, u32 channels, u32 width, u32 height) override;
    void DestroyGPUTexture(u64 textureHandle) override;

private:
    NvrhiVulkanState* state = nullptr;
};
