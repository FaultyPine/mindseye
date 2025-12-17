#include "me_texture.h"

#include "external/ktx/ktx.h"
#include "external/stb/stb_image.h"
#include "render/renderer_frontend.h"

void meTextureInitialize(EngineContext* ctx)
{
	ctx->textureSystem = MENEW(&ctx->engineArena, meTexturePool, &ctx->engineArena, &ctx->engineArena);
	meTexture badDataTexture = {};
	u8 dummyImgData[] = 
    {
        0,0,0,255, // black
        255,0,255,255, // hot pink
        255,0,255,255, // hot pink
        0,0,0,255, // black
    };
	u64 badDataTextureGPUHandle = ctx->renderer->UploadTextureToGPU(
		meSpan(dummyImgData, sizeof(dummyImgData)), 4, 2, 2);
	badDataTexture.buffer = meGPUBuffer{.bufferHandle = static_cast<u32>(badDataTextureGPUHandle)};
	StringCopy(StringView(badDataTexture.name, meTexture::METEXTURE_MAX_NAME_LEN), STRING_LIT("Bad Data"));
	meMaterialTextureType texType = meMaterialTextureType::Diffuse;
	StringView diffuseTexUniformName = meMaterialGetTextureTypeName(texType);
	badDataTexture.sampler = ctx->renderer->CreateShaderUniform(diffuseTexUniformName, meUniformDataType::UNIFORM_SAMPLER);
	ctx->textureSystem->badData = badDataTexture;
}

meTexturePool& meTextureGetPool()
{
	return *GetEngineCtx()->textureSystem;
}

static u32 GetChannelsFromTextureFormat(meTextureFormat format)
{
	switch (format)
	{
		case meTextureFormat_RGBA8: 
			return 4;
		default: ME_ASSERT(false);
	}
	return 0;
}

meGPUBuffer meTexturePool::Load(const meTextureLoadParams& params)
{
	u32 numChannels = GetChannelsFromTextureFormat(params.textureFormat);
	meGPUBuffer result = 
	{
		.bufferHandle = (u32)RendererGetMain().UploadTextureToGPU(params.mem, numChannels, params.width, params.height),
		.cpuData = params.mem,
	};
	return result;
}

meGPUBuffer meTexturePool::Load(
	RendererFrontend* renderer,
	StringView gltfResPath,
	const cgltf_image& gltfImage)
{
	meTexturePool& texturePool = meTextureGetPool();
	UNUSED_DECL meAllocator* texturePayloadAllocator = texturePool.GetPayloadAllocator();

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
        // specifying STBI_rgb_alpha means even for images without alpha channels, it'll fill in 255
		u8* pngDecompressed = stbi_load_from_memory((u8*)filebuf.data, filebuf.size, &w, &h, &channels, STBI_rgb_alpha);
		channels = 4; // channels will get set to the # channels in the source image, but i've asked stbi to fill in the alpha regardless to make things simple
        u64 pngDecompressedSize = w * h * channels;
		if (pngDecompressed == nullptr)
		{
			const char* loadFailure = stbi_failure_reason();
			LOG_ERROR("Failed to load img | %s", loadFailure);
			break;
		}
		meSpan decompressedImgMem = meSpan(pngDecompressed, pngDecompressedSize);
		resultGPUBuff.bufferHandle = renderer->UploadTextureToGPU(decompressedImgMem, channels, w, h);

		// TODO: make this async & use ktx instead of uncompressed img data
		// I.E. asset compilation pipeline (png -> ktx -> becomes (cached) runtime asset
		UNUSED_DECL auto writeKtx = +[](s32 channels, s32 w, s32 h, meSpan imgMem, StringView textureResourcePath, meAllocator* texturePayloadAllocator)
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
		//writeKtx(channels, w, h, decompressedImgMem, textureResourcePath, texturePayloadAllocator);

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
