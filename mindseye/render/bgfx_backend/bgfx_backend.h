#pragma once

#include "mindseye/render/renderer_frontend.h"

struct BgfxRendererBackend : public RendererFrontend
{
	virtual void PushLine(
		const glm::vec3& start,
		const glm::vec3& end,
		const glm::vec4& color) override;

	virtual void Push2DBox(
		const glm::vec2& start,
		const glm::vec2& end,
		const glm::vec4& color) override;

    void Initialize(EngineContext* engine) override;
    void Teardown(EngineContext* engine) override;
    void* RenderScene(RenderInput* scene) override;

	// each renderer backend is responsible for drawing the ImDrawData imgui produces
	virtual void BeginImguiContext() override;
	virtual void EndImguiContext() override;

	virtual u64 CreateVertexBuffer(meSpan bufferMem, meMeshVertexLayoutType layout) override;
	virtual u64 CreateShaderUniform(StringView name, meUniformDataType type) override;
	virtual u64 CreateShaderProgram(meSpan fsMem, meSpan vsMem) override;
	virtual void DestroyShaderProgram(u64 programHandle) override;
	// NOTE: textureMem must live at least 2 bgfx::frame calls past this point. (I.E. we do not copy the data, we give bgfx just the pointer...)
	virtual u64 UploadTextureToGPU(meSpan textureMem, u32 channels, u32 width, u32 height) override;
	virtual void DestroyGPUTexture(u64 textureHandle) override;

	void renderScreenSpaceQuad(const glm::mat4& proj, uint8_t _view, bgfx::ProgramHandle _program, float _x, float _y, float _width, float _height, bgfx::TextureHandle tex);

	bgfx::UniformHandle screenQuadSampler = bgfx::UniformHandle(bgfx::kInvalidHandle);
};