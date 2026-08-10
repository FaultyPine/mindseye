#include "me_material.h"

#include "external/cgltf.h"
#include "core/me_profile.h"
#include "render/renderer_frontend.h"
#include "shaders/generated/main_lit_fs.sc.h"
#include "shaders/generated/main_lit_vs.sc.h"

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
	meMaterialPool& materialPool = meMaterialGetPool();
	meMaterialID materialHdl = materialPool.Load({.resourceType = meResourceType_InstanceAsset});
	meMaterial& material = materialPool.Get(materialHdl);
	StringCopy(StringView(material.name, meMaterial::MEMATERIAL_MAX_NAME_LEN), StringFromCString(gltfMaterial.name));
	
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
			diffuseTextureMem = texturePool.Load(renderer, gltfResPath, *gltftex.image, textureHdl);
			StringCopy(StringView(texture.name, meTexture::METEXTURE_MAX_NAME_LEN), StringFromCString(gltftex.name));
		}
		else
		{
			// 1x1 pixel of a single color
			const float* rgba = gltfMaterial.pbr_metallic_roughness.base_color_factor;
			u32* textureData = MEALLOC(renderer->rendererPersistentAllocator, sizeof(u32));
			*textureData = PackFloatsToU32(rgba[0], rgba[1], rgba[2], rgba[3]);
			meSpan textureMem = meSpan(textureData, sizeof(*textureData));
			meTextureQueueGPUUpload(textureHdl, textureMem, 4, 1, 1);
			diffuseTextureMem = meGPUBuffer{.bufferHandle = U32_INVALID_ID, .cpuData = textureMem};
			StringCopy(StringView(texture.name, meTexture::METEXTURE_MAX_NAME_LEN), STRING_LIT("Static Color Texture"));
		}
		if (diffuseTextureMem.cpuData)
		{
			texture.buffer = diffuseTextureMem;
		}
		
		// TODO: deduplicate, see comment in me_resourcepool.h
		u64 litProgram = renderer->CreateShaderProgram(meSpan(main_lit_fs), meSpan(main_lit_vs));
		meShaderPool& shaderPool = meShaderGetPool();
		meShaderID shaderHandle = shaderPool.Load({ .resourceType = meResourceType_InstanceAsset });
		meShader& shader = shaderPool.Get(shaderHandle);
		shader.uniformHandles = DynArrayCreate<meShaderUniform>(shaderPool.resourcePayloadAllocator);
		meShaderUniform timeU = meShaderUniform();
		timeU.uniformData = MEALLOC(shaderPool.resourcePayloadAllocator, sizeof(glm::vec4));;
		timeU.handle = renderer->CreateShaderUniform(STRING_LIT("u_time"), meUniformDataType::UNIFORM_VEC4);
		SET_BIT(timeU.flags, meShaderFlags_AlwaysReupload, true);
		timeU.updateCb = + [](meShaderUniform* uniform, void* userData) {
			*((glm::vec4*)uniform->uniformData) = glm::vec4(GetTimeSec(), 0, 0, 0);
		};
		DynArrayPush(shader.uniformHandles, timeU);
		shader.program = litProgram;

		material.shaderHandle = meTypedAsset<MAShader>(meAsset(shaderHandle, MAShader));
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
