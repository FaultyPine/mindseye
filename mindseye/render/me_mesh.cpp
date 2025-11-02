#include "me_mesh.h"


void meMeshInitialize(EngineContext* engine)
{
	engine->meshSystem = MENEW(&engine->engineArena, meMeshPool, &engine->engineArena, &engine->engineArena);
}

meMeshPool& meMeshPoolGet()
{
	return *GetEngineCtx()->meshSystem;
}
