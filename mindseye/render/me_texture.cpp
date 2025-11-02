#include "me_texture.h"



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
	const bgfx::Memory* imgMem = bgfx::makeRef(dummyImgData, ARRAYSIZE(dummyImgData) * sizeof(u8));
	bgfx::TextureFormat::Enum format = bgfx::TextureFormat::Enum::RGBA8;
	bgfx::TextureHandle tex = bgfx::createTexture2D(
		2, 2, false, 1, format, BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE, imgMem);
	badDataTexture.buffer = meGPUBuffer{.bufferHandle = tex.idx};
	StringCopy(StringView(badDataTexture.name, meTexture::METEXTURE_MAX_NAME_LEN), STRING_LIT("Bad Data"));
	meMaterialTextureType texType = meMaterialTextureType::Diffuse;
	StringView diffuseTexUniformName = meMaterialGetTextureTypeName(texType);
	badDataTexture.sampler = bgfx::createUniform(diffuseTexUniformName.cstr(), bgfx::UniformType::Sampler).idx;
	ctx->textureSystem->badData = badDataTexture;
}

meTexturePool& meTextureGetPool()
{
	return *GetEngineCtx()->textureSystem;
}
