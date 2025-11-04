
#include "bgfx_backend.h"


#include "core/me_core.h"
#include "core/me_math.h"
#include "scene/me_scene.h"
#include "render/me_mesh.h"
#include "render/me_material.h"
#include "render/me_texture.h"
#include "core/me_string.h"
#include "scene/me_entity.h"

#include "external/cgltf.h"

#include "external/ktx/ktx.h"
#include "external/stb/stb_image.h"

#include "bgfx/bgfx/include/bgfx/bgfx.h"
#include "bgfx/bgfx/src/config.h"
#include "bgfx/bx/include/bx/bx.h"
#include "external/bgfx/bgfx/examples/common/imgui/bgfx_imgui.cpp"

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
    imguiCreate();
    engine->renderer->rendererLoggingEnabled = false; // tmp
	bgfx::touch(0);
	bgfx::frame();
}


void BgfxRendererBackend::Teardown(EngineContext* engine)
{
    imguiDestroy();
    bgfx::shutdown();
}

meGPUBuffer LoadTextureFromGLTF(
	RendererFrontend* renderer,
	StringView gltfResPath,
	const cgltf_image& gltfImage)
{
	meTexturePool& texturePool = meTextureGetPool();
	meAllocator* texturePayloadAllocator = texturePool.GetPayloadAllocator();

	meSpan imageData = {};
	meGPUBuffer resultGPUBuff = {};
	BREAKABLE_SCOPE
	u64 offset = 0;
	u64 size = 0;
	if (gltfImage.buffer_view)
	{
		cgltf_buffer_view* gltfImgBufferView = gltfImage.buffer_view;
		cgltf_buffer* buffer = gltfImgBufferView->buffer;
		ME_ASSERT(!buffer->data && "Embedded gltf data not supported. Needs to be in external file");
		offset = gltfImgBufferView->offset;
		size = gltfImgBufferView->size;
	}
	if (gltfImage.uri)
	{
		StringView textureUri = StringView(gltfImage.uri, CStringLength(gltfImage.uri));
		ME_ASSERT(FindInString(textureUri, STRING_LIT("data:")) == -1); // not supporting embedded texture data rn
		StringView gltfResDir = msFsGetDirFromPath(gltfResPath);
		StringView textureResourcePath = StringFormat("%.*s/%.*s", STRING_VAARGS(gltfResDir), STRING_VAARGS(textureUri));
		// TODO: offer a loading fast-path if an equivalent .ktx file is next to the source file
		OSFileReference file;
		if (!meOSOpenFile(file, textureResourcePath, OSFileFlags::OnlyIfExists))
		{
			LOG_ERROR("Failed to open texture file %.*s", STRING_VAARGS(textureResourcePath));
			break;
		}
		if (size == 0)
		{
			size = meOSGetFileSize(file);
		}
		if (offset != 0)
		{
			meOSSetFileCursor(file, offset, OSFileCursorMode::BEGIN);
		}
		Allocation filebuf = MEALLOC(GetTLScratch(), size);
		if (!meOSReadFileContents(file, filebuf, size))
		{
			LOG_ERROR("Failed to read texture file %.*s", STRING_VAARGS(textureResourcePath));
			break;
		}
		meOSCloseFile(file);

		// take loaded image -> decompress
		s32 w = 0; s32 h = 0; s32 channels = 0;
		// TODO: this uses malloc/free, make it use my allocators (texturePayloadAllocator)
		u8* pngDecompressed = stbi_load_from_memory((u8*)filebuf.data, filebuf.size, &w, &h, &channels, STBI_rgb_alpha);
		u64 pngDecompressedSize = w * h * channels;
		if (pngDecompressed == nullptr)
		{
			const char* loadFailure = stbi_failure_reason();
			LOG_ERROR("Failed to load img | %s", loadFailure);
			break;
		}
		meSpan decompressedImgMem = meSpan(pngDecompressed, pngDecompressedSize);
		resultGPUBuff.bufferHandle = renderer->UploadTextureToGPU(decompressedImgMem, channels, w, h);

		// TODO: make this async
		auto writeKtx = +[](s32 channels, s32 w, s32 h, meSpan imgMem, StringView textureResourcePath, meAllocator* texturePayloadAllocator)
		{ // decompressed image data -> ktx
			ktxTexture2* texture;
			ktxTextureCreateInfo createInfo;
			KTX_error_code result; UNUSED(result);
			ktx_uint32_t level, layer, faceSlice;
			ktx_size_t srcSize;
 
			u32 vkFormat = 43; //VK_FORMAT_R8G8B8A8_SRGB
			if (channels == 3)
			{
				//vkFormat = 29; // VK_FORMAT_R8G8B8_SRGB
				vkFormat = 27; // VK_FORMAT_R8G8B8_UINT
			}
			//createInfo.glInternalformat = GL_RGB8;   // Ignored if creating a ktxTexture2.
			createInfo.vkFormat = vkFormat;   // Ignored if creating a ktxTexture1.
			createInfo.baseWidth = w;
			createInfo.baseHeight = h;
			createInfo.baseDepth = 1;
			createInfo.numDimensions = 2;
			// Note: it is not necessary to provide a full mipmap pyramid.
			createInfo.numLevels = 1;//log2(createInfo.baseWidth) + 1;
			createInfo.numLayers = 1;
			createInfo.numFaces = 1;
			createInfo.isArray = KTX_FALSE;
			createInfo.generateMipmaps = KTX_FALSE; // generate later if needed
 
			// Call ktxTexture1_Create to create a KTX texture.
			result = ktxTexture2_Create(&createInfo,
										KTX_TEXTURE_CREATE_ALLOC_STORAGE,
										&texture);
			ME_ASSERT(result == KTX_SUCCESS);
			u8* src = (u8*)imgMem.data;
			srcSize = imgMem.size;
			level = 0;
			layer = 0;
			faceSlice = 0;
			result = ktxTexture_SetImageFromMemory(ktxTexture(texture),
												level, layer, faceSlice,
												src, srcSize);
			ME_ASSERT(result == KTX_SUCCESS);
			// Repeat for the other 15 slices of the base level and all other levels
			// up to createInfo.numLevels.

			u8* ktxApiMem = nullptr;
			u64 ktxMemSize = 0;
			// :/ can we reduce the number of redundant copies happening here?
			result = ktxTexture_WriteToMemory(ktxTexture(texture), &ktxApiMem, &ktxMemSize);
			ME_ASSERT(result == KTX_SUCCESS);
			Allocation ktxMem = MEALLOC(texturePayloadAllocator, ktxMemSize);
			ME_MEMCPY(ktxMem, ktxApiMem, ktxMemSize);

			// write to file
			StringView texUriWithoutExt = textureResourcePath;
			s32 texUriExtOffset = FindInString(textureResourcePath, STRING_LIT("."));
			if (texUriExtOffset != -1)
			{
				texUriWithoutExt = textureResourcePath.OffsetView(0, texUriExtOffset);
			}
			StringBuilder outTexName = StringBuilder(GetTLScratch());
			outTexName.Append(texUriWithoutExt);
			outTexName.Append(STRING_LIT(".ktx"));

			ktxTexture_WriteToNamedFile(ktxTexture(texture), outTexName.data);
			ktxTexture_Destroy(ktxTexture(texture));
		};
		writeKtx(channels, w, h, decompressedImgMem, textureResourcePath, texturePayloadAllocator);

		imageData = decompressedImgMem;
	}
	else
	{
		LOG_ERROR("Tried to load a texture without a file payload... so there's nothing to load?");
	}
	resultGPUBuff.cpuData = imageData;
	BREAKABLE_SCOPE_END

	return resultGPUBuff;
}

Eye LoadMaterialFromGLTF(
	RendererFrontend* renderer,
	StringView gltfResPath,
	const cgltf_material& gltfMaterial)
{
	meMaterialPool& materialPool = meMaterialGetPool();
	Eye materialHdl = materialPool.Create();
	meMaterial& material = materialPool.Get(materialHdl);
	StringCopy(StringView(material.name, meMaterial::MEMATERIAL_MAX_NAME_LEN), StringFromCString(gltfMaterial.name));
	
	if (gltfMaterial.has_pbr_metallic_roughness)
	{
		meTexturePool& texturePool = meTextureGetPool();
		Eye textureHdl = texturePool.Create();
		meTexture& texture = texturePool.Get(textureHdl);

		meMaterialTextureType texType = meMaterialTextureType::Diffuse;
		StringView diffuseTexUniformName = meMaterialGetTextureTypeName(texType);

		texture.sampler = bgfx::createUniform(diffuseTexUniformName.cstr(), bgfx::UniformType::Sampler).idx;
		material.textureHandles[texType] = textureHdl;

		meGPUBuffer diffuseTextureMem = {};
		if (gltfMaterial.pbr_metallic_roughness.base_color_texture.texture)
		{
			const cgltf_texture& gltftex = *gltfMaterial.pbr_metallic_roughness.base_color_texture.texture;
			diffuseTextureMem = LoadTextureFromGLTF(renderer, gltfResPath, *gltftex.image);
			StringCopy(StringView(texture.name, meTexture::METEXTURE_MAX_NAME_LEN), StringFromCString(gltftex.name));
		}
		else
		{
			// 1x1 pixel of a single color
			float* rgba = MEALLOC(renderer->rendererPersistentAllocator, sizeof(float) * 4);
			ME_MEMCPY((void*)rgba, &gltfMaterial.pbr_metallic_roughness.base_color_factor[0], sizeof(float) * 4);
			u32 textureData = PackFloatsToU32(rgba[0], rgba[1], rgba[2], rgba[3]);
			const bgfx::Memory* imgMem = bgfx::makeRef(&textureData, sizeof(textureData));
			bgfx::TextureFormat::Enum format = bgfx::TextureFormat::Enum::RGBA8;
			bgfx::TextureHandle tex = bgfx::createTexture2D(
				1, 1, false, 1, format, BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE, imgMem);
			diffuseTextureMem = meGPUBuffer{.bufferHandle = tex.idx, .cpuData = meSpan(rgba, sizeof(float) * 4)};
			StringCopy(StringView(texture.name, meTexture::METEXTURE_MAX_NAME_LEN), STRING_LIT("Static Color Texture"));
		}
		if (diffuseTextureMem.IsValid())
		{
			texture.buffer = diffuseTextureMem;
		}
		
		// TODO: deduplicate, see comment in me_resourcepool.h
		u64 litProgram = renderer->CreateShaderProgram(meSpan(main_lit_fs), meSpan(main_lit_vs));
		meShaderPool& shaderPool = meShaderGetPool();
		Eye shaderHandle = shaderPool.Create();
		meShader& shader = shaderPool.Get(shaderHandle);
		shader.uniformHandles = DynArrayCreate<meShaderUniform>(shaderPool.resourcePayloadAllocator);
		meShaderUniform timeU = meShaderUniform();
		timeU.uniformData = MEALLOC(shaderPool.resourcePayloadAllocator, sizeof(glm::vec4));;
		timeU.handle = bgfx::createUniform("u_time", bgfx::UniformType::Enum::Vec4).idx;
		SET_BIT(timeU.flags, meShaderFlags_AlwaysReupload, true);
		timeU.updateCb = + [](meShaderUniform* uniform, void* userData) {
			*((glm::vec4*)uniform->uniformData) = glm::vec4(GetTimeSec(), 0, 0, 0);
		};
		DynArrayPush(shader.uniformHandles, timeU);
		shader.program = litProgram;

		material.shaderHandle = shaderHandle;
	}
	return materialHdl;
}

void LoadMeshFromGLTF(
	RendererFrontend* renderer,
	StringView gltfResPath,
	meAllocator* meshPayloadAllocator,
	const cgltf_mesh& inMesh, 
	meMesh& outMesh)
{
	outMesh.name = String(StringFromCString(inMesh.name), renderer->rendererPersistentAllocator);
	BoundingBox& meshBounds = outMesh.meshBounds;
	for (u64 meshPrimIdx = 0; meshPrimIdx < inMesh.primitives_count; meshPrimIdx++)
	{
		const cgltf_primitive& prim = inMesh.primitives[meshPrimIdx];
		if (prim.material)
		{
			outMesh.materialHandle = LoadMaterialFromGLTF(renderer, gltfResPath, *prim.material);
		}
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
					meshBounds.min = glm::min(meshBounds.min, glm::make_vec3(dataUnit));
					meshBounds.max = glm::max(meshBounds.max, glm::make_vec3(dataUnit));
				}
				outMesh.vertBuffer.cpuData = allocation;
				bgfx::VertexLayout v_layout; 
				v_layout.begin()
				.add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
				.end();
				outMesh.vertBuffer.bufferHandle = bgfx::createVertexBuffer(bgfx::makeRef(outMesh.vertBuffer.cpuData, outMesh.vertBuffer.cpuData.size), v_layout).idx;
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
				bgfx::VertexLayout v_layout; 
				v_layout.begin()
				.add(bgfx::Attrib::Normal,  3, bgfx::AttribType::Float)
				.end();
				outMesh.normBuffer.bufferHandle = bgfx::createVertexBuffer(bgfx::makeRef(outMesh.normBuffer.cpuData, outMesh.normBuffer.cpuData.size), v_layout).idx;
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
				bgfx::VertexLayout v_layout; 
				v_layout.begin()
				.add(bgfx::Attrib::TexCoord0,  2, bgfx::AttribType::Float)
				.end();
				outMesh.texcoordBuffer.bufferHandle = bgfx::createVertexBuffer(bgfx::makeRef(outMesh.texcoordBuffer.cpuData, outMesh.texcoordBuffer.cpuData.size), v_layout).idx;
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
	bgfx::ProgramHandle program = bgfx::createProgram(vsHandle, fsHandle);
	return program.idx;
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

void BgfxRendererBackend::LoadSceneRuntime(meScene& outScene, meAllocator* sceneAllocator)
{
	if (outScene.IsValid())
	{
		const cgltf_scene& scene = *outScene.runtime.gltfData->scene;
		StringView gltfResPath = outScene.runtime.gltfResourcePath;
		meMeshPool& meshPool = meMeshPoolGet();
		outScene.runtime.entities = DynArrayCreate<EntityRef>(sceneAllocator);
		for (u64 nodeIdx = 0; nodeIdx < scene.nodes_count; nodeIdx++)
		{
			const cgltf_node& node = *scene.nodes[nodeIdx];
			float nodeMatrix[16];
			cgltf_node_transform_local(&node, nodeMatrix);
			Transform nodeTf = Transform(glm::make_mat4(nodeMatrix));
			EntityRef entityRef = Entity::CreateEntity(node.name, nodeTf);
			EntityData& entity = Entity::GetEntity(entityRef);
			const cgltf_mesh& gltfmesh = *node.mesh;
			Eye meshHandle = meshPool.Create();
			meMesh& mesh = meshPool.Get(meshHandle);
			meAllocator* meshPayloadAllocator = meshPool.resourcePayloadAllocator;
			LoadMeshFromGLTF(this, gltfResPath, meshPayloadAllocator, gltfmesh, mesh);
			entity.mesh = meshHandle;
			entity.bounds = mesh.meshBounds; // may change due to stuff like animations/etc. Default initialized to mesh bounds
			DynArrayPush(outScene.runtime.entities, entityRef);
		}
	}
}

void renderScreenSpaceQuad(uint8_t _view, bgfx::ProgramHandle _program, float _x, float _y, float _width, float _height, bgfx::TextureHandle tex = bgfx::TextureHandle(bgfx::kInvalidHandle));

void* BgfxRendererBackend::RenderScene(RenderInput* input)
{
	const SceneRuntimeData& sceneRuntime = input->scene.runtime;
	//u32 windowWidth = input->osData.windowWidth;
	//u32 windowHeight = input->osData.windowHeight;
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x443355FF, 1.0f, 0);
	bgfx::touch(0);

	for (DynArray_Foreach(sceneRuntime.entities, i))
	{
		const EntityRef& entityRef = sceneRuntime.entities[i];
		const EntityData& entity = Entity::GetEntity(entityRef);
		const Eye& meshHandle = entity.mesh;
		const meMesh& mesh = meMeshPoolGet().Get(meshHandle);
		if (mesh.IsLoaded())
		{
			// TODO: set uniforms
			const meTexturePool& texturePool = meTextureGetPool();
			const meMaterialPool& materialPool = meMaterialGetPool();
			const meMaterial& material = materialPool.Get(mesh.materialHandle);
			Eye diffuseTextureHdl = material.textureHandles[meMaterialTextureType::Diffuse];
			const meTexture& diffuseTex = texturePool.Get(diffuseTextureHdl);
			bgfx::TextureHandle bgfxDiffuseTex = bgfx::TextureHandle { static_cast<u16>(diffuseTex.buffer.bufferHandle) };
			meShader& shader = meShaderGetPool().Get(material.shaderHandle);
			for (DynArray_Foreach(shader.uniformHandles, uniformIdx))
			{
				meShaderUniform& uniform = shader.uniformHandles[uniformIdx];
				if (!uniform.RefreshInternalUniformData()) continue;
				bgfx::setUniform(bgfx::UniformHandle(uniform.handle), uniform.uniformData);
			}
			bgfx::ProgramHandle program = bgfx::ProgramHandle(shader.program);
			//renderScreenSpaceQuad(0, program, 0, 0, 256, 256, bgfxDiffuseTex);

			glm::mat4 modelMat = entity.transform.ToModelMatrix();
			bgfx::setTransform(&modelMat[0]);

			const meCamera& cam = input->scene.mainCamera;
			glm::mat4 proj = cam.GetProjectionMatrix();
			glm::mat4 view = cam.GetViewMatrix();
			bgfx::setViewTransform(0, glm::value_ptr(view), glm::value_ptr(proj));

			bgfx::setVertexBuffer(0, bgfx::VertexBufferHandle { static_cast<u16>(mesh.vertBuffer.bufferHandle) });
			bgfx::setVertexBuffer(1, bgfx::VertexBufferHandle { static_cast<u16>(mesh.normBuffer.bufferHandle) });
			bgfx::setVertexBuffer(2, bgfx::VertexBufferHandle { static_cast<u16>(mesh.texcoordBuffer.bufferHandle) });
			bgfx::setIndexBuffer(bgfx::IndexBufferHandle { static_cast<u16>(mesh.idxBuffer.bufferHandle) });
		
			bgfx::setTexture(0, bgfx::UniformHandle { static_cast<u16>(diffuseTex.sampler) }, bgfxDiffuseTex);
			bgfx::setState(BGFX_STATE_WRITE_RGB
						   | BGFX_STATE_WRITE_A
						   | BGFX_STATE_WRITE_Z
						   | BGFX_STATE_DEPTH_TEST_LESS
						   | BGFX_STATE_CULL_CCW
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
bgfx::VertexLayout PosTexCoord0Vertex::ms_layout;

void renderScreenSpaceQuad(uint8_t _view, bgfx::ProgramHandle _program, float _x, float _y, float _width, float _height, bgfx::TextureHandle tex)
{
	bgfx::TransientVertexBuffer tvb;
	bgfx::TransientIndexBuffer tib;
	static bgfx::UniformHandle sampler = bgfx::UniformHandle(bgfx::kInvalidHandle);
	if (sampler.idx == bgfx::kInvalidHandle)
	{
		PosTexCoord0Vertex::init();
		StringView uniformName = meMaterialGetTextureTypeName(Diffuse);
		sampler = bgfx::createUniform(uniformName.cstr(), bgfx::UniformType::Sampler);
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

		bgfx::setState(BGFX_STATE_WRITE_RGB 
					   | BGFX_STATE_WRITE_A 
					   | BGFX_STATE_WRITE_Z 
					   | BGFX_STATE_DEPTH_TEST_LESS 
					   | BGFX_STATE_CULL_CCW 
					   | BGFX_STATE_MSAA);
		bgfx::setTexture(0, sampler, tex);
		bgfx::setIndexBuffer(&tib);
		bgfx::setVertexBuffer(0, &tvb);
		bgfx::submit(_view, _program);
	}
}
