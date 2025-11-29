#pragma once

#include "me_entity.h"
#include "core/me_core.h"
#include "render/me_mesh.h"

namespace Entity
{

static EntityRegistry& GetRegistry()
{
    return *GetEngineCtx()->entityRegistry;
}

EntityRef GenerateEntityID(const char* name = nullptr)
{
    EntityRef result = 0;
    if (!name)
    {
        EntityRegistry& registry = GetRegistry();
        result = registry.entityCreationIndex;
    }
    else
    {
        result = HashBytes((u8*)name, strnlen(name, ENTITY_NAME_MAX_LENGTH));
    }
    ME_ASSERT(result != U32_INVALID_ID); // TODO: handle this gracefully
    return result;
}

void InitializeEntitySystem(Arena* arena)
{
    EntityRegistry* registryMem = MENEW(arena, EntityRegistry);
    GetEngineCtx()->entityRegistry = registryMem;
    // dummy entity with bad id so we can return it on failure from methods like GetEntity
    registryMem->entMap[U32_INVALID_ID] = {};
    Entity::SetFlag(U32_INVALID_ID, EntityFlags::DISABLED, true);
}

void SetFlag(EntityData& ent, EntityFlags flag, bool enabled)
{
    u32& bitfield = ent.flags;
    SET_BIT(bitfield, flag, enabled);
}

void SetFlag(EntityRef ent, EntityFlags flag, bool enabled)
{
    EntityRegistry& registry = GetRegistry();
    SetFlag(registry.entMap[ent], flag, enabled);
}

bool IsFlag(const EntityData& data, EntityFlags flag)
{
    const u32& bitfield = data.flags;
    bool result = TEST_BIT(bitfield, flag);
    return result;
}

bool IsFlag(EntityRef ent, EntityFlags flag)
{
    EntityRegistry& registry = GetRegistry();
    return IsFlag(registry.entMap[ent], flag);
}

EntityRef CreateEntity(
    const char* name, 
    const Transform& tf, 
    u32 flags)
{
    EntityRegistry& registry = GetRegistry();
    EntityData ent = {};
    ent.transform = tf;
    ent.flags = flags;
    u32 entityID = 0;
    if (name)
    {
        // if this entity has a name, use the name's hash as the id
        // this is so we can lookup entities by name
        size_t strlength = strnlen(name, ENTITY_NAME_MAX_LENGTH);
        ME_MEMCPY(ent.name, name, strlength);
        entityID = HashBytes((u8*)name, strlength);
    }
    else
    {
        // if no name, the entity id is just a incrementally increasing num
        ME_MEMCLEAR(ent.name, ENTITY_NAME_MAX_LENGTH);
        entityID = registry.entityCreationIndex;
    }
    // hash until we don't collide
    while (registry.entMap.count(entityID))
    {
        entityID = HashBytes((u8*)&entityID, sizeof(entityID));
    }
    ME_ASSERT(entityID != U32_INVALID_ID); // make absolutely sure
    // we increment this every time, even if it's not what we use for the id.
    registry.entityCreationIndex++;
    registry.entMap[entityID] = ent;
    return entityID;
}

bool DestroyEntity(EntityRef ent)
{
    EntityRegistry& registry = GetRegistry();
	EntityData& entity = GetEntity(ent);
	UNUSED(entity);
    //if (entity.id == U32_INVALID_ID)
    //{
    //    return false;
    //}
    //entity.mesh.Delete();
	UNIMPLEMENTED();
    registry.entMap.erase(ent);
    return true;
}

EntityData& GetEntity(EntityRef ref)
{
    EntityRegistry& registry = GetRegistry();
    if (registry.entMap.count(ref) > 0)
    {
        return registry.entMap[ref];
    }
    return registry.entMap[U32_INVALID_ID]; // if doesn't exist, return our dummy
}

EntityData& GetEntity(const char* name)
{
    u32 namehash = HashBytes((u8*)name, strnlen(name, ENTITY_NAME_MAX_LENGTH));
    EntityRegistry& registry = GetRegistry();
    if (registry.entMap.count(namehash) > 0)
    {
        return registry.entMap[namehash];
    }
    return registry.entMap[U32_INVALID_ID]; // if doesn't exist, return our dummy

}

bool HasRenderable(EntityRef ent)
{
    const EntityData& entity = GetEntity(ent);
    return (bool)entity.mesh;
}

bool AddRenderable(EntityRef ent, const Eye& mesh)
{
    EntityData& entity = GetEntity(ent);
    // if we already have a model, don't overwrite
    if (entity.mesh)
    {
        return false;
    }
    entity.mesh = mesh;
    return true;
}

void OverwriteRenderable(EntityRef ent, const Eye& mesh)
{
    EntityData& entity = GetEntity(ent);
    entity.mesh = mesh;
}

void GetRenderableEntities(EntityRef* dst, u32* numEntities)
{
    EntityRegistry& registry = GetRegistry();
    *numEntities = 0;
    for (const auto& [ref, ent] : registry.entMap)
    {
        if (!IsFlag(ent, EntityFlags::DISABLED) && ent.mesh)
        {
            if (dst)
            {
                dst[*numEntities] = ref;
            }
            (*numEntities)++;
        }
    }
}

void SetTransform(EntityRef ent, const Transform& tf)
{
    EntityRegistry& registry = GetRegistry();
    registry.entMap[ent].transform = tf;
}


} // namespace Entity