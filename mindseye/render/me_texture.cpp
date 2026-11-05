#include "core/me_app.h"
#include "core/me_profile.h"
#include "me_texture.h"

#include "render/renderer_frontend.h"

ME_DEFINE_LOG_CATEGORY(TextureSystem);

void meTextureInitialize(EngineContext* ctx)
{
    RendererFrontend& renderer = RendererGetMain();
    {
        ctx->textureSystem = MENEW(&ctx->engineArena, meTexturePool, &ctx->engineArena, &ctx->engineArena);
        meTexture badDataTexture = {};
        static u8 dummyImgData[] = 
        {
            0,0,0,255, // black
            255,0,255,255, // hot pink
            255,0,255,255, // hot pink
            0,0,0,255, // black
        };
        meSpan dummyImgDataSpan = meSpan(dummyImgData, sizeof(dummyImgData));
        u64 badDataTextureGPUHandle = ctx->renderer->UploadTextureToGPU(
            dummyImgDataSpan, 4, 2, 2);
        badDataTexture.buffer = meGPUBuffer{.bufferHandle = static_cast<u32>(badDataTextureGPUHandle), .cpuData = dummyImgDataSpan};
        StringCopy(StringView(badDataTexture.name, meTexture::METEXTURE_MAX_NAME_LEN), STRING_LIT("Bad Data"));
        meMaterialTextureType texType = meMaterialTextureType::Diffuse;
        StringView diffuseTexUniformName = meMaterialGetTextureTypeName(texType);
        badDataTexture.sampler = ctx->renderer->CreateShaderUniform(diffuseTexUniformName, meUniformDataType::UNIFORM_SAMPLER);
        badDataTexture.samplingFlags = ME_SAMPLER_POINT;
        ctx->textureSystem->GetBadData() = badDataTexture;
    }
    meTexturePool& texturePool = meTextureGetPool();
    
    {
        meTextureID plainWhite1x1Hdl = texturePool.Load({.resourceType = meResourceType_InstanceAsset});
        meTexture& plainWhite1x1 = texturePool.Get(plainWhite1x1Hdl);
    
        meMaterialTextureType texType = meMaterialTextureType::Diffuse;
        StringView diffuseTexUniformName = meMaterialGetTextureTypeName(texType);
        plainWhite1x1.sampler = renderer.CreateShaderUniform(diffuseTexUniformName, meUniformDataType::UNIFORM_SAMPLER);
    
        const glm::vec4 defaultRGBA = glm::vec4(1);
        const float* rgba = glm::value_ptr(defaultRGBA);
        u32* pixel = MEALLOC(renderer.rendererPersistentAllocator, sizeof(u32));
        *pixel = PackFloatsToU32(rgba[0], rgba[1], rgba[2], rgba[3]);
        meSpan pixels(pixel, sizeof(u32));
        u32 tex = renderer.UploadTextureToGPU(pixels, 4, 1, 1);
        meGPUBuffer diffuseTextureMem = meGPUBuffer{.bufferHandle = tex, .cpuData = pixels};
        if (diffuseTextureMem.IsValid())
        {
            plainWhite1x1.buffer = diffuseTextureMem;
        }
    
        ctx->textureSystem->plainWhite1x1 = plainWhite1x1Hdl;
    }
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
	ME_PROFILE_FUNCTION();
	u32 numChannels = GetChannelsFromTextureFormat(params.textureFormat);
	meGPUBuffer result = 
	{
		.bufferHandle = (u32)RendererGetMain().UploadTextureToGPU(params.mem, numChannels, params.width, params.height),
		.cpuData = params.mem,
	};
	return result;
}

struct meTextureAssetLoader : public meAssetLoader
{
    using meAssetLoader::meAssetLoader;

    bool meAssetInitializeRuntime(meAsset& asset) override
    {
        meTexture& texture = *(meTexture*)asset.runtimeContent.get();
        if (texture.buffer.bufferHandle == U32_INVALID_ID && texture.buffer.cpuData)
        {
            texture.buffer.bufferHandle = RendererGetMain().UploadTextureToGPU(texture.buffer.cpuData, 4, texture.width, texture.height);
        }
        return texture.buffer.bufferHandle != U32_INVALID_ID;
    }

    static void RegisterAssetLoader(meEventPayload payload)
    {
        meAllocator* allocator = (meAllocator*)payload.payload;
        meAssetRegisterLoader(MENEW(allocator, meTextureAssetLoader, &TD_METEXTURE, &meTextureGetPool(), meAssetType::MATexture));
    }
};

MEEVENT_REGISTER_STATIC(registerAssetLoader, meTextureAssetLoader::RegisterAssetLoader);
