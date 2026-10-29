#include "shaders/generated/main_lit_fs.sc.h"
#include "shaders/generated/main_lit_vs.sc.h"
#include "core/me_app.h"
#include "render/me_texture.h"
#include "core/me_profile.h"
#include "me_material.h"

#include "external/cgltf.h"
#include "render/renderer_frontend.h"

void meMaterialInitialize(EngineContext* ctx)
{
	ctx->materialSystem = MENEW(&ctx->engineArena, meMaterialPool, &ctx->engineArena, &ctx->engineArena);

	meMaterial badMaterial = {};
	StringCopy(StringView(badMaterial.name, meMaterial::MEMATERIAL_MAX_NAME_LEN), STRING_LIT("BadDataMaterial"));
	ctx->materialSystem->GetBadData() = badMaterial;
}

meMaterialPool& meMaterialGetPool()
{
	return *GetEngineCtx()->materialSystem;
}


StringView meMaterialGetTextureTypeName(meMaterialTextureType texType)
{
	switch (texType)
	{
		#define X(matname) case matname: return STRING_LIT("tex" #matname);
		ME_MATERIAL_TEXTURE_TYPE_NAMES
		#undef X
		default: return STRING_LIT("Unknown Texture Type");
	}
}

meMaterialID meMaterialPool::Load(
	RendererFrontend* renderer,
	StringView gltfResPath,
	const cgltf_material& gltfMaterial)
{
	ME_PROFILE_FUNCTION();
    meMaterialID materialHdl = meMaterialCreateDefault(StringFromCString(gltfMaterial.name));
	meMaterial& material = meMaterialGetPool().Get(materialHdl);

	if (gltfMaterial.has_pbr_metallic_roughness)
	{
		meTexturePool& texturePool = meTextureGetPool();
		meTextureID textureHdl = texturePool.Load({.resourceType = meResourceType_InstanceAsset});
		meTexture& texture = texturePool.Get(textureHdl);

		meMaterialTextureType texType = meMaterialTextureType::Diffuse;
		StringView diffuseTexUniformName = meMaterialGetTextureTypeName(texType);

		texture.sampler = renderer->CreateShaderUniform(diffuseTexUniformName, meUniformDataType::UNIFORM_SAMPLER);
		material.textureHandles[texType] = meTypedAsset<MATexture>(meAsset(textureHdl, MATexture));

		meGPUBuffer diffuseTextureMem = {};
		if (gltfMaterial.pbr_metallic_roughness.base_color_texture.texture)
		{
			const cgltf_texture& gltftex = *gltfMaterial.pbr_metallic_roughness.base_color_texture.texture;
			diffuseTextureMem = texturePool.Load(renderer, gltfResPath, *gltftex.image);
			StringCopy(StringView(texture.name, meTexture::METEXTURE_MAX_NAME_LEN), StringFromCString(gltftex.name));
		}
		else
		{
			// 1x1 pixel of a single color
            const float* rgba = gltfMaterial.pbr_metallic_roughness.base_color_factor;
            u32* pixel = MEALLOC(renderer->rendererPersistentAllocator, sizeof(u32));
            *pixel = PackFloatsToU32(rgba[0], rgba[1], rgba[2], rgba[3]);
            meSpan pixels(pixel, sizeof(u32));
            u32 tex = renderer->UploadTextureToGPU(pixels, 4, 1, 1);
            diffuseTextureMem = meGPUBuffer{.bufferHandle = tex, .cpuData = pixels};
			StringCopy(StringView(texture.name, meTexture::METEXTURE_MAX_NAME_LEN), STRING_LIT("Static Color Texture"));
		}
		if (diffuseTextureMem.IsValid())
		{
			texture.buffer = diffuseTextureMem;
		}
	}
	return materialHdl;
}

struct meMaterialAssetLoader : public meAssetLoader
{
	using meAssetLoader::meAssetLoader;

	static void RegisterAssetLoader(meEventPayload payload)
	{
		meAllocator* allocator = (meAllocator*)payload.payload;
		meAssetRegisterLoader(MENEW(allocator, meMaterialAssetLoader, &TD_MEMATERIAL, &meMaterialGetPool(), meAssetType::MAMaterial));
	}
};

MEEVENT_REGISTER_STATIC(registerAssetLoader, meMaterialAssetLoader::RegisterAssetLoader);

meMaterialID meMaterialCreateDefault(StringView name, glm::vec3 color)
{
    meMaterialPool& materialPool = meMaterialGetPool();
	meMaterialID materialHdl = materialPool.Load({.resourceType = meResourceType_InstanceAsset});
	meMaterial& material = materialPool.Get(materialHdl);
    StringCopy(StringView(material.name, meMaterial::MEMATERIAL_MAX_NAME_LEN), name);
    material.albedoTint = color;
    meShaderID shaderHandle = meShaderCreate(meSpan(main_lit_fs), meSpan(main_lit_vs));
    material.shaderHandle = meTypedAsset<MAShader>(meAsset(shaderHandle, MAShader));
    material.textureHandles[Diffuse] = meTextureGetPool().plainWhite1x1;

    return materialHdl;
}
