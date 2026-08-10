#pragma once

#include "scene/me_scene.h"
#include "render/me_shader.h"
#include "render/me_mesh.h"
#include "editor/me_editor.h"
struct EngineContext;

enum RendererBackendType
{
    NONE,
    VULKAN,
    BGFX,
    NVRHI_VULKAN,
};

#if !defined(RENDERER_BACKEND)
#define RENDERER_BACKEND (RendererBackendType::NVRHI_VULKAN)
#endif

// after creation, intended as a readonly container
// of *everything* the renderer needs to render *any* frame.
struct RenderInput
{
	EditorContext editorCtx;
	OSStateView osData = {}; // window width/height, mouse state, etc
	meScene scene = {};
};

struct RendererFrontend
{
    RendererBackendType backendType = NONE;
    meAllocator* rendererPersistentAllocator = nullptr;
    Arena rendererFrameArena = {};
    bool rendererLoggingEnabled = false;

	virtual void PushLine(
		const glm::vec3& start,
		const glm::vec3& end,
		const glm::vec4& color) {}
	
	virtual void Push2DBox(
		const glm::vec2& start,
		const glm::vec2& end,
		const glm::vec4& color) {}


    virtual void Initialize(EngineContext* engine) {}
    virtual void Teardown(EngineContext* engine) {}

    // takes in all the input the renderer needs to render a frame. Outputs a framebuffer (handle)
    virtual void* RenderScene(RenderInput* input) { return nullptr; }
    virtual void PresentFrame() {}

	// each renderer backend is responsible for drawing the ImDrawData imgui produces
	virtual void BeginImguiContext() = 0;
	virtual void EndImguiContext() = 0;

	// the vertex layout is a bitfield of all interleaved types. Use NTH_BIT to bitwise-or together the desired buffer types
	virtual u64 CreateVertexBuffer(meSpan bufferMem, meMeshVertexLayoutType layout) { return U32_INVALID_ID; }
	virtual u64 CreateShaderUniform(StringView name, meUniformDataType type) { return U32_INVALID_ID; }
	virtual u64 CreateShaderProgram(meSpan fsMem, meSpan vsMem) { return U32_INVALID_ID; }
	virtual void DestroyShaderProgram(u64 programHandle) {}
	virtual u64 UploadTextureToGPU(meSpan textureMem, u32 channels, u32 width, u32 height) { return U32_INVALID_ID; }
	virtual void DestroyGPUTexture(u64 textureHandle) {}
};

void RendererInitialize(EngineContext* engine);
void RendererTeardown();
RendererFrontend& RendererGetMain();
