#include "me_material.h"


void meMaterialInitialize(EngineContext* engine)
{
	engine->materialSystem = MENEW(&engine->engineArena, meMaterialPool, &engine->engineSceneAllocator, &engine->engineSceneAllocator);
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