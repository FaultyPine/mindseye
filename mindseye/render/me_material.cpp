#include "me_material.h"


void meMaterialInitialize(EngineContext* engine)
{
	engine->materialSystem = MENEW(&engine->engineArena, meMaterialPool, &engine->engineSceneAllocator, &engine->engineSceneAllocator);
}

meMaterialPool& meMaterialGetPool()
{
	return *GetEngineCtx()->materialSystem;
}