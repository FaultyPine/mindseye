
#include "bgfx_backend.h"


#include "core/me_core.h"
#include "core/me_math.h"
#include "scene/me_scene.h"
#include "render/me_mesh.h"

#include "external/cgltf.h"

#include "bgfx/bgfx/include/bgfx/bgfx.h"
#include "bgfx/bx/include/bx/bx.h"
#include "external/bgfx/bgfx/examples/common/imgui/bgfx_imgui.cpp"

// ---- shaders
#include "shaders/fs.h"
#include "shaders/vs.h"
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
		bx::debugPrintf("Fatal error: 0x%08x: %s", _code, _str);

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
    init.type = bgfx::RendererType::OpenGL;
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
    imguiCreate();
    engine->renderer->rendererLoggingEnabled = false; // tmp
}


void BgfxRendererBackend::Teardown(EngineContext* engine)
{
    imguiDestroy();
    bgfx::shutdown();
}


void LoadMeshFromGLTF(
	meAllocator* meshPayloadAllocator,
	const cgltf_mesh& inMesh, 
	meMesh& outMesh)
{
	bgfx::VertexLayout v_layout; 
	v_layout.begin()
	.add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
	.add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
	.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	.add(bgfx::Attrib::Normal,    3, bgfx::AttribType::Float)
	.end();
	for (u64 meshPrimIdx = 0; meshPrimIdx < inMesh.primitives_count; meshPrimIdx++)
	{
		const cgltf_primitive& prim = inMesh.primitives[meshPrimIdx];

		// attribs like position, texcoords, normals
		for (u64 attributeIdx = 0; attributeIdx < prim.attributes_count; attributeIdx++)
		{
			const cgltf_attribute& attrib = prim.attributes[attributeIdx];
			StringView attribName = StringView(attrib.name, CStringLength(attrib.name));
			if (attrib.type == cgltf_attribute_type_position)
			{
				const cgltf_accessor* accessor = attrib.data;
				u64 stride = sizeof(f32) * 3;
				u64 dataSize = stride * accessor->count;
				Allocation allocation = MEALLOC(meshPayloadAllocator, dataSize);
				Allocation bumper = allocation;
				f32 dataUnit[3];
				for (u64 posIdx = 0; posIdx < accessor->count; posIdx++)
				{
					cgltf_accessor_read_float(accessor, posIdx, dataUnit, 3);
					ME_MEMCPY(bumper, dataUnit, stride);
					bumper = bumper.Subspan(stride);
				}
				outMesh.vertBuffer.cpuData = allocation;
				outMesh.vertBuffer.bufferHandle = bgfx::createVertexBuffer(bgfx::makeRef(outMesh.vertBuffer.cpuData, dataSize), v_layout).idx;
			}
			else if (attrib.type == cgltf_attribute_type_normal)
			{
				const cgltf_accessor* accessor = attrib.data;
				u64 stride = sizeof(f32) * 3;
				u64 dataSize = stride * accessor->count;
				Allocation allocation = MEALLOC(meshPayloadAllocator, dataSize);
				Allocation bumper = allocation;
				f32 dataUnit[3];
				for (u64 posIdx = 0; posIdx < accessor->count; posIdx++)
				{
					cgltf_accessor_read_float(accessor, posIdx, dataUnit, 3);
					ME_MEMCPY(bumper, dataUnit, stride);
					bumper = bumper.Subspan(stride);
				}
				outMesh.normBuffer.cpuData = allocation;
			}
			else if (attrib.type == cgltf_attribute_type_tangent)
			{

			}
			else if (attrib.type == cgltf_attribute_type_texcoord)
			{
				const cgltf_accessor* accessor = attrib.data;
				u64 stride = sizeof(f32) * 2;
				u64 dataSize = stride * accessor->count;
				Allocation allocation = MEALLOC(meshPayloadAllocator, dataSize);
				Allocation bumper = allocation;
				f32 dataUnit[2];
				for (u64 posIdx = 0; posIdx < accessor->count; posIdx++)
				{
					cgltf_accessor_read_float(accessor, posIdx, dataUnit, 2);
					ME_MEMCPY(bumper, dataUnit, stride);
					bumper = bumper.Subspan(stride);
				}
				outMesh.texcoordBuffer.cpuData = allocation;
			}
		}

		// indices
		if (prim.indices)
		{
			u64 stride = prim.indices->stride;
			u64 indicesMemSize = prim.indices->count * stride;
			Allocation indicesMemory = MEALLOC(meshPayloadAllocator, indicesMemSize);
			meSpan indicesBumper = indicesMemory;
			for (u64 idx = 0; idx < prim.indices->count; idx++)
			{
				u64 readIdx = cgltf_accessor_read_index(prim.indices, idx);
				ME_MEMCPY(indicesBumper.data, &readIdx, stride);
				indicesBumper = indicesBumper.Subspan(stride);
			}
			outMesh.idxBuffer.cpuData = indicesMemory;
			outMesh.idxBuffer.bufferHandle = bgfx::createIndexBuffer(bgfx::makeRef(outMesh.idxBuffer.cpuData, indicesMemSize)).idx;
		}
	}
}

void* BgfxRendererBackend::RenderScene(RenderInput* input)
{
	static bgfx::ProgramHandle program;
	meAllocator* meshPayloadAllocator = this->rendererPersistentAllocator;
	const cgltf_scene& scene = *input->scene.runtime.gltfData->scene;
	static meMesh mesh = {};
	for (u64 nodeIdx = 0; nodeIdx < scene.nodes_count; nodeIdx++)
	{
		const cgltf_node& node = *scene.nodes[nodeIdx];

		const cgltf_mesh& gltfmesh = *node.mesh;
		if (!mesh.IsLoaded())
		{
			LoadMeshFromGLTF(meshPayloadAllocator, gltfmesh, mesh);
			const bgfx::Memory* fsmem = bgfx::alloc(sizeof(fs)+1);
			ME_MEMCPY(fsmem->data, fs, sizeof(fs));
			fsmem->data[fsmem->size-1] = '\0';

			const bgfx::Memory* vsmem = bgfx::alloc(sizeof(vs)+1);
			ME_MEMCPY(vsmem->data, vs, sizeof(vs));
			vsmem->data[vsmem->size-1] = '\0';

			bgfx::ShaderHandle fsHandle = bgfx::createShader(fsmem);
			bgfx::ShaderHandle vsHandle = bgfx::createShader(vsmem);
			program = bgfx::createProgram(vsHandle, fsHandle);
			bgfx::setState(BGFX_STATE_WRITE_RGB
						   | BGFX_STATE_WRITE_A
						   | BGFX_STATE_WRITE_Z
						   | BGFX_STATE_DEPTH_TEST_LESS
						   | BGFX_STATE_CULL_CCW
						   | BGFX_STATE_MSAA);
		}
	}

	

	u32 windowWidth = input->osData.windowWidth;
	u32 windowHeight = input->osData.windowHeight;
    const MouseState& mouseState = input->osData.mouseState;
    //const bgfx::Stats* stats = bgfx::getStats();
    // Set view and clear
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x443355FF, 1.0f, 0);
 
    imguiBeginFrame(mouseState.mouseX
        ,  mouseState.mouseY
        ,  (TEST_BIT(mouseState.buttons, MouseState::LBUTTON) ? IMGUI_MBUT_LEFT   : 0)
			| (TEST_BIT(mouseState.buttons, MouseState::RBUTTON) ? IMGUI_MBUT_RIGHT  : 0)
			| (TEST_BIT(mouseState.buttons, MouseState::MBUTTON) ? IMGUI_MBUT_MIDDLE : 0)
        , mouseState.scroll
		, u16(windowWidth)
		, u16(windowHeight)
        );

    imguiEndFrame();
	bgfx::touch(0);
	//bgfx::setDebug(BGFX_DEBUG_PROFILER | BGFX_DEBUG_STATS | BGFX_DEBUG_TEXT);

	const bx::Vec3 at  = { 0.0f, 1.0f,  0.0f };
	const bx::Vec3 eye = { 0.0f, 1.0f, -2.5f };

	// Set view and projection matrix for view 0.
	{
		float view[16];
		bx::mtxLookAt(view, eye, at);

		float proj[16];
		bx::mtxProj(proj, 60.0f, float(windowWidth)/float(windowHeight), 0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);
		bgfx::setViewTransform(0, view, proj);

		// Set view 0 default viewport.
		//bgfx::setViewRect(0, 0, 0, uint16_t(m_width), uint16_t(m_height) );
	}

	float mtx[16];
	bx::mtxRotateXY(mtx
					, 0.0f
					, GetTimeSec()*0.8f
					);
	bgfx::setTransform(mtx);

	bgfx::setVertexBuffer(0, bgfx::VertexBufferHandle { static_cast<u16>(mesh.vertBuffer.bufferHandle) });
	bgfx::setIndexBuffer(bgfx::IndexBufferHandle { static_cast<u16>(mesh.idxBuffer.bufferHandle) });
	bgfx::submit(0, program);


    ArenaClear(&rendererFrameArena);
    bgfx::frame();
    return nullptr;
}





struct PosColorTexCoord0Vertex
{
	float m_x;
	float m_y;
	float m_z;
	uint32_t m_abgr;
	float m_u;
	float m_v;
	float m_blend;
	float m_angle;

	static void init()
	{
		ms_layout
		.begin()
		.add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
		.add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
		.add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
		.end();
	}

	static bgfx::VertexLayout ms_layout;
};
bgfx::VertexLayout PosColorTexCoord0Vertex::ms_layout;

void renderScreenSpaceQuad(uint8_t _view, bgfx::ProgramHandle _program, float _x, float _y, float _width, float _height)
{
	bgfx::TransientVertexBuffer tvb;
	bgfx::TransientIndexBuffer tib;

	if (bgfx::allocTransientBuffers(&tvb, PosColorTexCoord0Vertex::ms_layout, 4, &tib, 6) )
	{
		PosColorTexCoord0Vertex* vertex = (PosColorTexCoord0Vertex*)tvb.data;

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
		vertex[0].m_abgr = 0xff0000ff;
		vertex[0].m_u = minu;
		vertex[0].m_v = minv;

		vertex[1].m_x = maxx;
		vertex[1].m_y = miny;
		vertex[1].m_z = zz;
		vertex[1].m_abgr = 0xff00ff00;
		vertex[1].m_u = maxu;
		vertex[1].m_v = minv;

		vertex[2].m_x = maxx;
		vertex[2].m_y = maxy;
		vertex[2].m_z = zz;
		vertex[2].m_abgr = 0xffff0000;
		vertex[2].m_u = maxu;
		vertex[2].m_v = maxv;

		vertex[3].m_x = minx;
		vertex[3].m_y = maxy;
		vertex[3].m_z = zz;
		vertex[3].m_abgr = 0xffffffff;
		vertex[3].m_u = minu;
		vertex[3].m_v = maxv;

		uint16_t* indices = (uint16_t*)tib.data;

		indices[0] = 0;
		indices[1] = 2;
		indices[2] = 1;
		indices[3] = 0;
		indices[4] = 3;
		indices[5] = 2;

		bgfx::setState(BGFX_STATE_DEFAULT);
		bgfx::setIndexBuffer(&tib);
		bgfx::setVertexBuffer(0, &tvb);
		bgfx::submit(_view, _program);
	}
}
