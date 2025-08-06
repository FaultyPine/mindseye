#pragma once

#include "core/me_defines.h"
#include "scene/me_transform.h"
struct Arena;
struct Model;

enum EntityFlags
{
    DISABLED = 1,

    NUM_ENTITY_FLAGS,
};
STATIC_ASSERT(NUM_ENTITY_FLAGS < 32);

typedef u32 EntityRef;
#define ENTITY_NAME_MAX_LENGTH 50
struct EntityData
{
    Transform transform = {};
    //Model model = {};
    BoundingBox bounds = {};
    EntityRef id = U32_INVALID_ID;
    u32 flags = 0;
    s8 name[ENTITY_NAME_MAX_LENGTH];
    
    operator bool() { return isValid(); }
    EntityData() = default;
    inline bool isValid() { return id != U32_INVALID_ID; }
};
#include <unordered_map>
// BOOKMARK: Use custom map here
typedef std::unordered_map<u32, EntityData> EntityMap;

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
MEAPI void SetTransform(EntityRef ent, const Transform& tf);
MEAPI bool HasRenderable(EntityRef ent);
MEAPI bool AddRenderable(EntityRef ent, const Model& model);
MEAPI void OverwriteRenderable(EntityRef ent, const Model& model);
// if dst is nullptr, this returns the *number* of renderable entities
// if dst is not nullptr, we fill in the buffer
MEAPI void GetRenderableEntities(EntityRef* dst, u32* numEntities);

} // namespace Entity

