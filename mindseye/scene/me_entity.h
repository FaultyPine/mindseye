#pragma once

#include "core/me_defines.h"
#include "scene/me_transform.h"
#include "core/containers/me_map.h"
#include "core/me_core.h"
#include "asset/me_asset.h"
#include "render/me_mesh.h"
struct Arena;

typedef u32 EntityFlags;
enum EntityFlags_
{
	EntityFlags_Invalid = 1,
    EntityFlags_DISABLED,
	EntityFlags_HIDDEN,
    EntityFlags_Selected,

    EntityFlags_NUM_ENTITY_FLAGS,
};
STATIC_ASSERT(EntityFlags_NUM_ENTITY_FLAGS < 32);

typedef Eye EntityRef;

struct MEREFLECT(type) meEntity
{
    ME_ASSET_STRUCTURE(meEntity);
    
    meTransform transform = {};
    MEREFLECT(tooltip = "The 'appearance' of the entity")
    meTypedAsset<MAMesh> mesh = {};
	// gameplay-focused bounds. Rendering bounds including anims may be different (stored on mesh)
    BoundingBox authoritativeBounds = {}; 
    EntityFlags flags = 0;
	String name = {};
    
};


struct meEntityPool : public meResourcePool<meEntity>
{
	meEntityPool(
		meAllocator* resourceAllocator,
		meAllocator* payloadAllocator) :
		meResourcePool<meEntity>(resourceAllocator, payloadAllocator) {}

	meAssetType GetAssetType() const
	{
		return MAEntity;
	}
};

void InitializeEntitySystem(EngineContext* engine);
void DeinitializeEntitySystem(EngineContext* engine);
meEntityPool& meEntityGetPool();

MEAPI meTypedAsset<MAEntity> meEntityCreateBlankInstance(
	StringView name = {});
MEAPI bool meEntityDestroy(EntityRef ent);
MEAPI meEntity& meEntityGet(EntityRef ent);

MEAPI void meEntitySetFlag(EntityRef ent, EntityFlags flag, bool enabled);
MEAPI void meEntitySetFlag(meEntity& ent, EntityFlags flag, bool enabled);
MEAPI bool meEntityIsFlag(EntityRef ent, EntityFlags flag);
MEAPI bool meEntityIsFlag(const meEntity& ent, EntityFlags flag);


