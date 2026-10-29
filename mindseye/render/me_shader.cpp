#include "render/renderer_frontend.h"
#include "core/me_app.h"
#include "me_shader.h"
#include "shaders/generated/baddata_fs.sc.h"
#include "shaders/generated/baddata_vs.sc.h"
#include "asset/me_asset.h"


void meShaderSetupDefaultUniforms(meShader& shader)
{
    if (DynArrayGetSize(shader.uniformHandles) == 0)
    {
        RendererFrontend& renderer = RendererGetMain();
        meShaderPool& shaderPool = meShaderGetPool();
        meShaderUniform timeU = meShaderUniform();
        timeU.uniformData = MEALLOC(shaderPool.resourcePayloadAllocator, sizeof(glm::vec4));
        timeU.handle = renderer.CreateShaderUniform(STRING_LIT("u_time"), meUniformDataType::UNIFORM_VEC4);
        SET_BIT(timeU.flags, meShaderFlags_AlwaysReupload, true);
        timeU.updateCb = + [](meShaderUniform* uniform, const meMaterial& material, void* userData) {
            *((glm::vec4*)uniform->uniformData) = glm::vec4(GetTimeSec(), 0, 0, 0);
        };
        DynArrayPush(shader.uniformHandles, timeU);

        meShaderUniform albedoTint = meShaderUniform();
        albedoTint.uniformData = MEALLOC(shaderPool.resourcePayloadAllocator, sizeof(glm::vec4));
        albedoTint.handle = renderer.CreateShaderUniform(STRING_LIT("albedoTint"), meUniformDataType::UNIFORM_VEC4);
        SET_BIT(albedoTint.flags, meShaderFlags_AlwaysReupload, true);
        albedoTint.updateCb = + [](meShaderUniform* uniform, const meMaterial& material, void* userData) {
            *((glm::vec4*)uniform->uniformData) = glm::vec4(material.albedoTint, 1.0f);
        };
        DynArrayPush(shader.uniformHandles, albedoTint);
    }
}

void meShaderInitialize(EngineContext* ctx)
{
	ctx->shaderSystem = MENEW(&ctx->engineArena, meShaderPool, &ctx->engineArena, &ctx->engineArena);
	
	// create a "null" shader
    meShader& badShader = ctx->shaderSystem->GetBadData();
    badShader.program = ctx->renderer->CreateShaderProgram(baddata_fs, baddata_vs);
    badShader.uniformHandles = DynArrayCreate<meShaderUniform>(ctx->shaderSystem->resourcePayloadAllocator);
    meShaderSetupDefaultUniforms(badShader);
}

meShaderID meShaderCreate(meSpan shaderBytecodeFS, meSpan shaderBytecodeVS)
{
    u64 shaderHash = HashBytes(shaderBytecodeVS, shaderBytecodeVS.size);
    shaderHash = HashCombine(shaderHash, HashBytes(shaderBytecodeFS, shaderBytecodeFS.size));
    meShaderPool& shaderPool = meShaderGetPool();
    auto it = shaderPool.shaderCache.find(shaderHash);
    if (it != shaderPool.shaderCache.end())
    {
        return it->second;
    }
    meShaderID shaderHdl = shaderPool.Load({.resourceType = meResourceType_InstanceAsset});
    meShader& shader = shaderPool.Get(shaderHdl);
    shader.program = RendererGetMain().CreateShaderProgram(shaderBytecodeFS, shaderBytecodeVS);
	shader.uniformHandles = DynArrayCreate<meShaderUniform>(GetEngineCtx()->shaderSystem->resourcePayloadAllocator);
    shaderPool.shaderCache[shaderHash] = shaderHdl;
    meShaderSetupDefaultUniforms(shader);
    return shaderHdl;
}

meShaderPool& meShaderGetPool()
{
	return *GetEngineCtx()->shaderSystem;
}


u32 UniformDataTypeToSize(meUniformDataType type)
{
	static const u32 UniformDataTypeSizes[NUM_UNIFORM_DATA_TYPES] = 
	{
		0,
		sizeof(f32),
		sizeof(u32),
		sizeof(s32),
		sizeof(f32) * 2, // vec2
		sizeof(f32) * 3, // vec3
		sizeof(f32) * 4, // vec4
		sizeof(f32) * 9, // mat3
		sizeof(f32) * 16, // mat4
	};
	return UniformDataTypeSizes[(s32)type];
}

struct meShaderAssetLoader : public meAssetLoader
{
	using meAssetLoader::meAssetLoader;

	static void RegisterAssetLoader(meEventPayload payload)
	{
		meAllocator* allocator = (meAllocator*)payload.payload;
		meAssetRegisterLoader(MENEW(allocator, meShaderAssetLoader, &TD_MESHADER, &meShaderGetPool(), meAssetType::MAShader));
	}
};

MEEVENT_REGISTER_STATIC(registerAssetLoader, meShaderAssetLoader::RegisterAssetLoader);
