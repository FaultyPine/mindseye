#include "me_shader.h"
#include "shaders/generated/baddata_fs.sc.h"
#include "shaders/generated/baddata_vs.sc.h"

void meShaderInitialize(EngineContext* ctx)
{
	ctx->shaderSystem = MENEW(&ctx->engineArena, meShaderPool, &ctx->engineArena, &ctx->engineArena);
	
	// create a "null" shader
	meShader badShader = {}; 
	badShader.program = ctx->renderer->CreateShaderProgram(baddata_fs, baddata_vs);
	badShader.uniformHandles = DynArrayCreate<meShaderUniform>(ctx->shaderSystem->resourcePayloadAllocator);
	ctx->shaderSystem->GetBadData() = badShader;
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
