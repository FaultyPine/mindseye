#include "me_texture.h"



void meTextureInitialize(EngineContext* ctx)
{
	ctx->textureSystem = MENEW(&ctx->engineArena, meTexturePool, &ctx->engineSceneAllocator, &ctx->engineSceneAllocator);
}



meTexturePool& meTextureGetPool()
{
	return *GetEngineCtx()->textureSystem;
}