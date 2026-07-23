
#include "bgfx_backend.h"


#include "core/me_core.h"
#include "core/me_math.h"
#include "scene/me_scene.h"
#include "render/me_mesh.h"
#include "render/me_material.h"
#include "render/me_texture.h"
#include "core/me_string.h"
#include "scene/me_entity.h"
#include "core/me_scope_exit.h"

#include "external/cgltf.h"

#include "bgfx/bgfx/include/bgfx/bgfx.h"
#include "bgfx/bgfx/src/config.h"
#include "bgfx/bx/include/bx/bx.h"

// Stub ImGuizmo functions (we don't use them yet)
namespace ImGuizmo {
	void Create() {}
	void Destroy() {}
	void BeginFrame() {}
}

// Stub ImGui docking functions (we don't use docking yet)
namespace ImGui {
	void InitDockContext() {}
	void ShutdownDockContext() {}
}

#ifdef COMPILER_CLANG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#include "external/bgfx/bgfx/examples/common/imgui/imgui.cpp"
#include "external/bgfx/bgfx/examples/common/debugdraw/debugdraw.h"
#include "external/bgfx/bgfx/examples/common/debugdraw/debugdraw.cpp"
#ifdef COMPILER_CLANG
#pragma GCC diagnostic pop
#endif

// ---- shaders
#include "shaders/generated/main_lit_fs.sc.h"
#include "shaders/generated/main_lit_vs.sc.h"
// -------------

void OnWindowResize(int width, int height)
{
    bgfx::reset(width, height);
    bgfx::setViewRect(0, 0, 0, width, height);
}


struct BgfxCallback : public bgfx::CallbackI
{
	virtual ~BgfxCallback()
	{
	}

	virtual void fatal(const char* _filePath, uint16_t _line, bgfx::Fatal::Enum _code, const char* _str) override
	{
		BX_UNUSED(_filePath, _line);

		// Something unexpected happened, inform user and bail out.
		bx::debugPrintf("Fatal error: 0x%08x: %s\n", _code, _str);
		ME_ASSERT(false);
		// Must terminate, continuing will cause crash anyway.
		abort();
	}

	virtual void traceVargs(const char* _filePath, uint16_t _line, const char* _format, va_list _argList) override
	{
        if (GetEngineCtx()->renderer->rendererLoggingEnabled)
        {
            bx::debugPrintf("%s (%d): ", _filePath, _line);
            bx::debugPrintfVargs(_format, _argList);
        }
	}

	virtual void profilerBegin(const char* /*_name*/, uint32_t /*_abgr*/, const char* /*_filePath*/, uint16_t /*_line*/) override
	{
	}

	virtual void profilerBeginLiteral(const char* /*_name*/, uint32_t /*_abgr*/, const char* /*_filePath*/, uint16_t /*_line*/) override
	{
	}

	virtual void profilerEnd() override
	{
	}

	virtual uint32_t cacheReadSize(uint64_t _id) override
	{
        return 0;
	}

	virtual bool cacheRead(uint64_t _id, void* _data, uint32_t _size) override
	{
        return false;
	}

	virtual void cacheWrite(uint64_t _id, const void* _data, uint32_t _size) override
	{
	}

	virtual void screenShot(const char* _filePath, uint32_t _width, uint32_t _height, uint32_t _pitch, const void* _data, uint32_t /*_size*/, bool _yflip) override
	{
	}

	virtual void captureBegin(uint32_t _width, uint32_t _height, uint32_t /*_pitch*/, bgfx::TextureFormat::Enum /*_format*/, bool _yflip) override
	{
	}

	virtual void captureEnd() override
	{
	}

	virtual void captureFrame(const void* _data, uint32_t /*_size*/) override
	{
	}

};

void BgfxRendererBackend::Initialize(EngineContext* engine)
{
	// TODO: swap out the rendererPersistentAllocator for a tcmalloc esc heap.
    rendererPersistentAllocator = MENEW(&engine->engineArena, Arena, MEGABYTES_BYTES(50), "Renderer Persistent", &engine->engineArena);
    rendererFrameArena = ArenaInit(MEGABYTES_BYTES(10), "Renderer Frame", rendererPersistentAllocator);
    // If multiple systems are trying to subscribe here, it's time to make this an actual event, rather than one fn ptr
    ME_ASSERT(!engine->osData->onResizeCB);
    engine->osData->onResizeCB = OnWindowResize;
    bgfx::Init init;
    init.type = bgfx::RendererType::Vulkan;
    init.vendorId = BGFX_PCI_ID_NONE; // prioritize integrated? discrete? microsft/nvidia/amd adapter? None means do it automatically
    init.platformData.ndt = nullptr;
    init.platformData.nwh = engine->osData->hwnd; // bgfx renderer backend needs platform window handle, this is hardcoded to windows rn. If another platform is supported in the future, this'll throw a compiler error
    init.resolution.width = engine->osData->windowWidth;
    init.resolution.height = engine->osData->windowHeight;
    init.resolution.reset = BGFX_RESET_VSYNC;
    init.callback = MENEW(rendererPersistentAllocator, BgfxCallback);
    bgfx::init(init);
    bgfx::setDebug(BGFX_DEBUG_TEXT);
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x443355FF, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, init.resolution.width, init.resolution.height);
	ddInit(); // TODO: uses malloc/free for debugdraw, override with my own stuff
    imguiCreate();
	bgfx::touch(0);
	bgfx::frame();
}


static void DestroyMeshGPUResources(meMesh& mesh)
{
	if (mesh.vertBuffer.bufferHandle != U32_INVALID_ID)
	{
		bgfx::destroy(bgfx::VertexBufferHandle{ static_cast<u16>(mesh.vertBuffer.bufferHandle) });
		mesh.vertBuffer.bufferHandle = U32_INVALID_ID;
	}
	if (mesh.idxBuffer.bufferHandle != U32_INVALID_ID)
	{
		bgfx::destroy(bgfx::IndexBufferHandle{ static_cast<u16>(mesh.idxBuffer.bufferHandle) });
		mesh.idxBuffer.bufferHandle = U32_INVALID_ID;
	}
	if (mesh.normBuffer.bufferHandle != U32_INVALID_ID)
	{
		bgfx::destroy(bgfx::VertexBufferHandle{ static_cast<u16>(mesh.normBuffer.bufferHandle) });
		mesh.normBuffer.bufferHandle = U32_INVALID_ID;
	}
	if (mesh.texcoordBuffer.bufferHandle != U32_INVALID_ID)
	{
		bgfx::destroy(bgfx::VertexBufferHandle{ static_cast<u16>(mesh.texcoordBuffer.bufferHandle) });
		mesh.texcoordBuffer.bufferHandle = U32_INVALID_ID;
	}
}

static void DestroyTextureGPUResources(meTexture& tex)
{
	if (tex.buffer.bufferHandle != U32_INVALID_ID)
	{
		bgfx::destroy(bgfx::TextureHandle{ static_cast<u16>(tex.buffer.bufferHandle) });
		tex.buffer.bufferHandle = U32_INVALID_ID;
	}
	if (tex.sampler != U64_INVALID_ID)
	{
		bgfx::destroy(bgfx::UniformHandle{ static_cast<u16>(tex.sampler) });
		tex.sampler = U64_INVALID_ID;
	}
}

static void DestroyShaderGPUResources(meShader& shader)
{
	if (shader.program != 0)
	{
		bgfx::destroy(bgfx::ProgramHandle{ static_cast<u16>(shader.program) });
		shader.program = 0;
	}
	for (DynArray_Foreach(shader.uniformHandles, i))
	{
		meShaderUniform& uniform = shader.uniformHandles[i];
		if (uniform.handle != 0)
		{
			bgfx::destroy(bgfx::UniformHandle{ static_cast<u16>(uniform.handle) });
			uniform.handle = 0;
		}
	}
}

void BgfxRendererBackend::Teardown(EngineContext* engine)
{
	// Destroy all GPU resources from pools before shutting down bgfx
	{
		meMeshPool& meshPool = meMeshPoolGet();
		// NOTE: mesh badData is a copy of a pool entry (see meMeshInitialize),
		// so we skip it here to avoid double-destroying the same handles.
		for (auto& slot : meshPool.resourcePoolInstances)
			DestroyMeshGPUResources(slot.obj);
		for (auto& slot : meshPool.resourcePoolTemplates)
			DestroyMeshGPUResources(slot.obj);
	}
	{
		meTexturePool& texPool = meTextureGetPool();
		DestroyTextureGPUResources(texPool.GetBadData());
		for (auto& slot : texPool.resourcePoolInstances)
			DestroyTextureGPUResources(slot.obj);
		for (auto& slot : texPool.resourcePoolTemplates)
			DestroyTextureGPUResources(slot.obj);
	}
	{
		meShaderPool& shaderPool = meShaderGetPool();
		DestroyShaderGPUResources(shaderPool.GetBadData());
		for (auto& slot : shaderPool.resourcePoolInstances)
			DestroyShaderGPUResources(slot.obj);
		for (auto& slot : shaderPool.resourcePoolTemplates)
			DestroyShaderGPUResources(slot.obj);
	}

	// Destroy the screen-space quad sampler if it was created
	if (screenQuadSampler.idx != bgfx::kInvalidHandle)
		bgfx::destroy(screenQuadSampler);

	ddShutdown();
    imguiDestroy();
    bgfx::shutdown();
}

cgltf_material GenerateDummyGLTFMaterial()
{
	cgltf_material mat;
	ME_MEMCLEAR(&mat, sizeof(mat));
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wwritable-strings"
	mat.name = "NoMaterial";
	#pragma clang diagnostic pop
	return mat;
}

void BgfxRendererBackend::BeginImguiContext()
{
	OSStateView& osData = *GetEngineCtx()->osData;
	u32 windowWidth = osData.windowWidth;
	u32 windowHeight = osData.windowHeight;
    const meMouseInput& mouseState = osData.mouseState;
    imguiBeginFrame(mouseState.mousePosScreen.x
					,  mouseState.mousePosScreen.y
					,  (mouseState.IsMouseButtonDown(meMouseButton::LBUTTON) ? IMGUI_MBUT_LEFT   : 0)
					| (mouseState.IsMouseButtonDown(meMouseButton::RBUTTON) ? IMGUI_MBUT_RIGHT  : 0)
					| (mouseState.IsMouseButtonDown(meMouseButton::MBUTTON) ? IMGUI_MBUT_MIDDLE : 0)
					, mouseState.scroll
					, u16(windowWidth)
					, u16(windowHeight)
					);
}

void BgfxRendererBackend::EndImguiContext()
{
    imguiEndFrame();
}

static bgfx::UniformType::Enum meShaderUniformTypeToBgfx(meUniformDataType type)
{
	switch (type)
	{
		case meUniformDataType::UNIFORM_FLOAT:
		case meUniformDataType::UNIFORM_VEC4:
		case meUniformDataType::UNIFORM_VEC3:
		case meUniformDataType::UNIFORM_VEC2:
			return bgfx::UniformType::Enum::Vec4;
		case meUniformDataType::UNIFORM_SAMPLER:
			return bgfx::UniformType::Enum::Sampler;
		case meUniformDataType::UNIFORM_MAT3:
			return bgfx::UniformType::Enum::Mat3;
		case meUniformDataType::UNIFORM_MAT4:
			return bgfx::UniformType::Enum::Mat4;
		default: ME_ASSERT(false);
	}
	ME_ASSERT(false);
	return bgfx::UniformType::Enum::Sampler;
}

u64 BgfxRendererBackend::CreateVertexBuffer(
	meSpan bufferMem, 
	meMeshVertexLayoutType layout)
{
	if (TEST_BIT(layout, meMeshVertexLayoutType_Index16) || TEST_BIT(layout, meMeshVertexLayoutType_Index32))
	{
		// if Index bit is specified, no other bits may be specified
		// (Cannot interleave index buffers with other data)
		ME_ASSERT((layout & (~(NTH_BIT(meMeshVertexLayoutType_Index16) | NTH_BIT(meMeshVertexLayoutType_Index32)))) == 0);
		u32 result = bgfx::createIndexBuffer(bgfx::makeRef(bufferMem.data, bufferMem.size), TEST_BIT(layout, meMeshVertexLayoutType_Index32) ? BGFX_BUFFER_INDEX32 : BGFX_BUFFER_NONE).idx;
		return result;
	}
	bgfx::VertexLayout v_layout; 
	v_layout.begin();
	if (TEST_BIT(layout, meMeshVertexLayoutType_Position)) v_layout.add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float);
	if (TEST_BIT(layout, meMeshVertexLayoutType_Normal)) v_layout.add(bgfx::Attrib::Normal,  3, bgfx::AttribType::Float);
	if (TEST_BIT(layout, meMeshVertexLayoutType_Tangent)) v_layout.add(bgfx::Attrib::Tangent,  3, bgfx::AttribType::Float);
	if (TEST_BIT(layout, meMeshVertexLayoutType_TexCoord0)) v_layout.add(bgfx::Attrib::TexCoord0,  2, bgfx::AttribType::Float);
	if (TEST_BIT(layout, meMeshVertexLayoutType_Color)) v_layout.add(bgfx::Attrib::Color0,  4, bgfx::AttribType::Float);
	if (TEST_BIT(layout, meMeshVertexLayoutType_Weights)) v_layout.add(bgfx::Attrib::Weight,  1, bgfx::AttribType::Float);
	v_layout.end();
	u32 result = bgfx::createVertexBuffer(bgfx::makeRef(bufferMem.data, bufferMem.size), v_layout).idx;
	return result;
}

u64 BgfxRendererBackend::CreateShaderUniform(
	StringView name, 
	meUniformDataType type)
{
	u64 result = bgfx::createUniform(name.cstr(), meShaderUniformTypeToBgfx(type)).idx;
	return result;
}

u64 BgfxRendererBackend::CreateShaderProgram(meSpan fsMem, meSpan vsMem)
{
	const bgfx::Memory* fsmem = bgfx::alloc(fsMem.size+1);
	ME_MEMCPY(fsmem->data, fsMem.data, fsMem.size);
	fsmem->data[fsmem->size-1] = '\0';

	const bgfx::Memory* vsmem = bgfx::alloc(vsMem.size+1);
	ME_MEMCPY(vsmem->data, vsMem.data, vsMem.size);
	vsmem->data[vsmem->size-1] = '\0';

	bgfx::ShaderHandle fsHandle = bgfx::createShader(fsmem);
	bgfx::ShaderHandle vsHandle = bgfx::createShader(vsmem);
	bgfx::ProgramHandle program = bgfx::createProgram(vsHandle, fsHandle, true);
	if (!bgfx::isValid(program))
	{
		LOG_ERROR("Shader program was ill-formed somehow...");
	}
	return program.idx;
}

void BgfxRendererBackend::DestroyShaderProgram(u64 programHandle)
{
	bgfx::destroy(bgfx::ProgramHandle{ static_cast<u16>(programHandle) });
}

u64 BgfxRendererBackend::UploadTextureToGPU(meSpan textureMem, u32 channels, u32 width, u32 height)
{
	const bgfx::Memory* imgMem = bgfx::makeRef(textureMem.data, textureMem.size);
	bgfx::TextureFormat::Enum format = bgfx::TextureFormat::Enum::RGBA8;
	if (channels == 3)
	{
		format = bgfx::TextureFormat::Enum::RGB8;
	}
	bgfx::TextureHandle tex = bgfx::createTexture2D(
		width, height, false, 1, format, BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE, imgMem);
	return tex.idx;
}

void BgfxRendererBackend::DestroyGPUTexture(u64 textureHandle)
{
	bgfx::destroy(static_cast<bgfx::TextureHandle>(textureHandle));
}

void* BgfxRendererBackend::RenderScene(RenderInput* input)
{
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x443355FF, 1.0f, 0);
	bgfx::touch(0);

	DebugDrawEncoder dde;
	dde.begin(0);
	dde.drawAxis(0.0f, 0.0f, 0.0f);
	dde.drawGrid(Axis::Y, { 0.0f, 0.0f, 0.0f }, 50);
	ME_ON_SCOPE_EXIT([&dde](){ dde.end(); });

	
	// TODO: editor toggle?
    //const meCamera& cam = input->scene.mainCamera;
	const meCamera& cam = input->editorCtx.editorCamera;

    glm::mat4 projectionFromView = cam.GetProjectionFromViewMatrix();
    glm::mat4 viewFromWorld = cam.GetViewFromWorldMatrix();
	bgfx::setViewTransform(0, glm::value_ptr(viewFromWorld), glm::value_ptr(projectionFromView));

	for (DynArray_Foreach(input->scene.entities, i))
	{
		const meTypedAsset<MAEntity>& entityRef = input->scene.entities[i];
		ScopedAssetLockR<meEntity> entity(entityRef);
		if (!entity || meEntityIsFlag(*entity, EntityFlags_HIDDEN))
		{
			continue;
		}
		ScopedAssetLockR<meMesh> mesh(entity->mesh);
		if (mesh && mesh->IsLoaded())
		{
			ScopedAssetLockR<meMaterial> material(mesh->material);
			if (!material)
			{
				continue;
			}
			const meTypedAsset<MATexture>& diffuseTexture = material->textureHandles[meMaterialTextureType::Diffuse];
			ScopedAssetLockR<meTexture> diffuseTex(diffuseTexture);
			bgfx::TextureHandle bgfxDiffuseTex = {};
			if (diffuseTex)
			{
				bgfxDiffuseTex = bgfx::TextureHandle { static_cast<u16>(diffuseTex->buffer.bufferHandle) };
			}
			ScopedAssetLockW<meShader> shader(material->shaderHandle);
			if (!shader)
			{
				continue;
			}
			for (DynArray_Foreach(shader->uniformHandles, uniformIdx))
			{
				meShaderUniform& uniform = shader->uniformHandles[uniformIdx];
				if (!uniform.RefreshInternalUniformData()) continue;
				bgfx::setUniform(bgfx::UniformHandle(uniform.handle), uniform.uniformData);
			}

			bgfx::ProgramHandle program = bgfx::ProgramHandle(shader->program);
			//renderScreenSpaceQuad(proj, 0, program, 0, 0, 256, 256, bgfxDiffuseTex);

			glm::mat4 worldFromModel = entity->transform.ToWorldFromModelMatrix();
			bgfx::setTransform(&worldFromModel[0]);

			bgfx::setVertexBuffer(0, bgfx::VertexBufferHandle { static_cast<u16>(mesh->vertBuffer.bufferHandle) });
			if (mesh->idxBuffer.IsValid())
			{ // meshes without index buffers are valid, and used for generated shapes meshes
				bgfx::setIndexBuffer(bgfx::IndexBufferHandle { static_cast<u16>(mesh->idxBuffer.bufferHandle) });
			}
			if (mesh->normBuffer.IsValid())
			{
				bgfx::setVertexBuffer(1, bgfx::VertexBufferHandle { static_cast<u16>(mesh->normBuffer.bufferHandle) });
			}
			if (mesh->texcoordBuffer.IsValid())
			{
				bgfx::setVertexBuffer(2, bgfx::VertexBufferHandle { static_cast<u16>(mesh->texcoordBuffer.bufferHandle) });
			}
			if (diffuseTex && diffuseTex->IsValid())
			{
				bgfx::setTexture(0, bgfx::UniformHandle { static_cast<u16>(diffuseTex->sampler) }, bgfxDiffuseTex, diffuseTex->samplingFlags);
			}
			bgfx::setState(BGFX_STATE_WRITE_RGB
						   | BGFX_STATE_WRITE_A
						   | BGFX_STATE_WRITE_Z
						   | BGFX_STATE_DEPTH_TEST_LESS
						   | BGFX_STATE_CULL_CW
						   | BGFX_STATE_MSAA);
			bgfx::submit(0, program);
		}
	}

    ArenaClear(&rendererFrameArena);
    bgfx::frame();

    return nullptr;
}

struct PosTexCoord0Vertex
{
	float m_x;
	float m_y;
	float m_z;
	float m_u;
	float m_v;

	static void init()
	{
		ms_layout
		.begin()
		.add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
		.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		.end();
	}

	static bgfx::VertexLayout ms_layout;
};

void BgfxRendererBackend::PushLine(
	const glm::vec3& start,
	const glm::vec3& end,
	const glm::vec4& color)
{
	UNIMPLEMENTED();
}

void BgfxRendererBackend::Push2DBox(
		const glm::vec2& start,
		const glm::vec2& end,
		const glm::vec4& color) 
{
	bgfx::TextureHandle tex = bgfx::TextureHandle { static_cast<u16>(meTextureGetPool().GetBadData().buffer.bufferHandle) };
	renderScreenSpaceQuad(
		glm::mat4(), 0, 
		bgfx::ProgramHandle(), start.x, start.y, end.x - start.x, end.y - start.y, tex);
}

bgfx::VertexLayout PosTexCoord0Vertex::ms_layout;

void BgfxRendererBackend::renderScreenSpaceQuad(const glm::mat4& proj,
						   uint8_t _view, 
						   bgfx::ProgramHandle _program, 
						   float _x, float _y, float _width, float _height, 
						   bgfx::TextureHandle tex)
{
	bgfx::TransientVertexBuffer tvb;
	bgfx::TransientIndexBuffer tib;
	if (screenQuadSampler.idx == bgfx::kInvalidHandle)
	{
		PosTexCoord0Vertex::init();
		StringView uniformName = meMaterialGetTextureTypeName(Diffuse);
		screenQuadSampler = bgfx::createUniform(uniformName.cstr(), bgfx::UniformType::Sampler);
	}
	if (bgfx::allocTransientBuffers(&tvb, PosTexCoord0Vertex::ms_layout, 4, &tib, 6) )
	{
		
		PosTexCoord0Vertex* vertex = (PosTexCoord0Vertex*)tvb.data;

		float zz = 0.0f;

		const float minx = _x;
		const float maxx = _x + _width;
		const float miny = _y;
		const float maxy = _y + _height;

		float minu = -1.0f;
		float minv = -1.0f;
		float maxu =  1.0f;
		float maxv =  1.0f;

		vertex[0].m_x = minx;
		vertex[0].m_y = miny;
		vertex[0].m_z = zz;
		vertex[0].m_u = minu;
		vertex[0].m_v = minv;

		vertex[1].m_x = maxx;
		vertex[1].m_y = miny;
		vertex[1].m_z = zz;
		vertex[1].m_u = maxu;
		vertex[1].m_v = minv;

		vertex[2].m_x = maxx;
		vertex[2].m_y = maxy;
		vertex[2].m_z = zz;
		vertex[2].m_u = maxu;
		vertex[2].m_v = maxv;

		vertex[3].m_x = minx;
		vertex[3].m_y = maxy;
		vertex[3].m_z = zz;
		vertex[3].m_u = minu;
		vertex[3].m_v = maxv;

		uint16_t* indices = (uint16_t*)tib.data;

		indices[0] = 0;
		indices[1] = 2;
		indices[2] = 1;
		indices[3] = 0;
		indices[4] = 3;
		indices[5] = 2;

		glm::mat4 modelMat = glm::mat4();
		bgfx::setTransform(&modelMat[0]);

		glm::mat4 view = glm::mat4();
		bgfx::setViewTransform(0, glm::value_ptr(view), glm::value_ptr(proj));


		bgfx::setState(BGFX_STATE_WRITE_RGB 
					   | BGFX_STATE_WRITE_A 
					   | BGFX_STATE_WRITE_Z 
					   | BGFX_STATE_DEPTH_TEST_LESS 
					   //| BGFX_STATE_CULL_CCW 
					   | BGFX_STATE_MSAA);
		bgfx::setTexture(0, screenQuadSampler, tex);
		bgfx::setIndexBuffer(&tib);
		bgfx::setVertexBuffer(0, &tvb);
		bgfx::submit(_view, _program);
	}
}
