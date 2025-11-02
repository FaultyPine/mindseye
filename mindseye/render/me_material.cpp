#include "me_material.h"


void meMaterialInitialize(EngineContext* ctx)
{
	ctx->materialSystem = MENEW(&ctx->engineArena, meMaterialPool, &ctx->engineArena, &ctx->engineArena);
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