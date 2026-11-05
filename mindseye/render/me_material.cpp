#include "shaders/generated/main_lit_fs.sc.h"
#include "shaders/generated/main_lit_vs.sc.h"
#include "core/me_app.h"
#include "render/me_texture.h"
#include "core/me_profile.h"
#include "me_material.h"

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

struct meMaterialAssetLoader : public meAssetLoader
{
    using meAssetLoader::meAssetLoader;

    bool meAssetInitializeRuntime(meAsset& asset) override
    {
        meMaterial& material = *(meMaterial*)asset.runtimeContent.get();
        if (!material.shaderHandle.runtimeHandle && !material.shaderHandle.id)
        {
            meShaderID litShader = meShaderCreate(meSpan(main_lit_fs), meSpan(main_lit_vs));
            material.shaderHandle = meTypedAsset<MAShader>(meAsset(litShader, MAShader));
        }
        for (u32 i = 0; i < NUM_MATERIAL_TEXTURE_TYPES; i++)
        {
            meTextureID textureHandle = material.textureHandles[i].runtimeHandle;
            if (!textureHandle)
            {
                continue;
            }
            meTexturePool& textures = meTextureGetPool();
            meAsset& textureAsset = material.textureHandles[i];
            textureAsset.runtimeContent = textures.GetShared(textureHandle);
            if (!meAssetSystemGet().assetLoaders[MATexture]->meAssetInitializeRuntime(textureAsset))
            {
                return false;
            }
            meTexture& texture = textures.Get(textureHandle);
            if (texture.sampler == U64_INVALID_ID)
            {
                texture.sampler = RendererGetMain().CreateShaderUniform(
                    meMaterialGetTextureTypeName((meMaterialTextureType)i), UNIFORM_SAMPLER);
            }
        }
        return true;
    }

    static void RegisterAssetLoader(meEventPayload payload)
    {
        meAllocator* allocator = (meAllocator*)payload.payload;
        meAssetRegisterLoader(
            MENEW(allocator, meMaterialAssetLoader, &TD_MEMATERIAL, &meMaterialGetPool(), meAssetType::MAMaterial));
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
