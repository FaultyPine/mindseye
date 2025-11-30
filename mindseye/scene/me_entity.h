#pragma once

#include "core/me_defines.h"
#include "scene/me_transform.h"
#include "core/containers/me_map.h"
#include "core/me_core.h"
struct Arena;

typedef u32 EntityFlags;
enum EntityFlags_
{
    EntityFlags_DISABLED = 1,
	EntityFlags_HIDDEN,

    EntityFlags_NUM_ENTITY_FLAGS,
};
STATIC_ASSERT(EntityFlags_NUM_ENTITY_FLAGS < 32);

typedef u32 EntityRef;
#define ENTITY_NAME_MAX_LENGTH 50
struct EntityData
{
    Transform transform = {};
    Eye mesh = {};
	// gameplay-focused bounds. Rendering bounds including anims may be different (stored on mesh)
    BoundingBox authoritativeBounds = {}; 
    u32 flags = 0;
    s8 name[ENTITY_NAME_MAX_LENGTH];
    
    EntityData() = default;
};

typedef meMap<EntityRef, EntityData> EntityMap;

// at the moment, we keep a registry allocated with the current scene
// this currently contains all entities at all times
// This registry isn't meant to be used to know "what entities should I render"
// or queries like that. For those, there's another structure of entity
// references in the meScene
struct EntityRegistry
{
    EntityMap entMap = {};
    u32 entityCreationIndex = 0;
};

namespace Entity
{

void InitializeEntitySystem(Arena* arena);

MEAPI EntityRef CreateEntity(
    const char* name, 
    const Transform& tf = {}, 
    u32 flags = 0);
MEAPI bool DestroyEntity(EntityRef ent);
MEAPI EntityData& GetEntity(EntityRef ent);
MEAPI EntityData& GetEntity(const char* name);

MEAPI void SetFlag(EntityRef ent, EntityFlags flag, bool enabled);
MEAPI void SetFlag(EntityData& ent, EntityFlags flag, bool enabled);
MEAPI bool IsFlag(EntityRef ent, EntityFlags flag);
MEAPI bool IsFlag(const EntityData& ent, EntityFlags flag);

} // namespace Entity

