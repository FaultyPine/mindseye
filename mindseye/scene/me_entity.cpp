#pragma once

#include "me_entity.h"
#include "core/me_core.h"
#include "core/me_app.h"
#include "render/me_mesh.h"

meEntityPool& meEntityGetPool()
{
    return *GetEngineCtx()->entityPool;
}

void InitializeEntitySystem(EngineContext* engine)
{
	engine->entityPool = MENEW(&engine->engineArena, meEntityPool, &engine->engineArena, &engine->engineArena);
    meEntitySetFlag(EYE_INVALID, EntityFlags_Invalid, true);
    meEntitySetFlag(EYE_INVALID, EntityFlags_DISABLED, true);
}

void DeinitializeEntitySystem(EngineContext* ctx)
{
    ctx->entityPool->Clear();
    MEDELETE(&ctx->engineArena, meEntityPool, ctx->entityPool);
    ctx->entityPool = nullptr;
}

void meEntitySetFlag(meEntity& ent, EntityFlags flag, bool enabled)
{
    u32& bitfield = ent.flags;
    SET_BIT(bitfield, flag, enabled);
}

void meEntitySetFlag(EntityRef ent, EntityFlags flag, bool enabled)
{
    meEntitySetFlag(meEntityGet(ent), flag, enabled);
}

bool meEntityIsFlag(const meEntity& data, EntityFlags flag)
{
    const u32& bitfield = data.flags;
    bool result = TEST_BIT(bitfield, flag);
    return result;
}

bool meEntityIsFlag(EntityRef ent, EntityFlags flag)
{
    return meEntityIsFlag(meEntityGet(ent), flag);
}

meTypedAsset<MAEntity> meEntityCreateBlankInstance(
	StringView name)
{
    meAsset newEntityAsset = meAssetCreateNewInstanceAsset(MAEntity);
    meEntityPool& entityPool = meEntityGetPool();
    meEntity& ent = entityPool.Get(newEntityAsset);
    if (name)
    {
		ent.name = name;
    }
    else
    {
		ent.name = StringFormatTmp("UnnamedEntity%i", (u32)newEntityAsset.runtimeHandle);
    }
    // default bounds is bounds of default mesh
    ent.authoritativeBounds = meMeshPoolGet().Get(ent.mesh).meshBounds;
    return newEntityAsset;
}

bool meEntityDestroy(EntityRef ent)
{
    meEntityPool& entityPool = meEntityGetPool();
    entityPool.Destroy(ent);
    return true;
}

meEntity& meEntityGet(EntityRef ref)
{
    meEntityPool& entityPool = meEntityGetPool();
    return entityPool.Get(ref);
}


struct meEntityAssetLoader : public meAssetLoader
{
	using meAssetLoader::meAssetLoader;

	static void RegisterAssetLoader(meEventPayload payload)
	{
		meAllocator* allocator = (meAllocator*)payload.payload;
		meAssetRegisterLoader(MENEW(allocator, meEntityAssetLoader, &TD_MEENTITY, &meEntityGetPool(), meAssetType::MAEntity));
	}
};

MEEVENT_REGISTER_STATIC(registerAssetLoader, meEntityAssetLoader::RegisterAssetLoader);