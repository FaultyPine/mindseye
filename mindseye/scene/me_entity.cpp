#pragma once

#include "me_entity.h"
#include "core/me_core.h"
#include "render/me_mesh.h"

StringView EntityRefSerializerToStringFn(
	const meTypeDescriptor& typeDescriptor,
	meAllocator* allocator, 
	meSpan data)
{
	EntityRef* ref = (EntityRef*)data.data;
	EntityData& entity = Entity::GetEntity(*ref);
	if (TEST_BIT(entity.flags, EntityFlags_Invalid))
	{
		return STRING_LIT("INVALID_ENTITY");
	}
	meSpan entitySpan = SPAN_FROM(entity);
	return TD_ENTITYDATA.ToString(allocator, entitySpan);
}

bool EntityRefDeserializerFromStringFn(
	const meTypeDescriptor& typeDescriptor,
	DeserializeContext& ctx)
{
	// we serialize/deserialize EntityRef as if it were EntityData
	DeserializeContext entityDataCtx = ctx;
	EntityData entity = {};
	entityDataCtx.outputData = SPAN_FROM(entity);
	if (!TD_ENTITYDATA.FromString(entityDataCtx))
	{
		LOG_ERROR("Failed to deserialize entity");
		return false;
	}
	EntityRef ref = Entity::CreateEntity(entity.name, entity.transform, entity.flags);
	ME_ASSERT(ctx.outputData.size == sizeof(ref.ref));
	ME_MEMCPY(ctx.outputData.data, &ref.ref, sizeof(ref.ref));
	return true;
}

namespace Entity
{

static EntityRegistry& GetRegistry()
{
    return *GetEngineCtx()->entityRegistry;
}

void InitializeEntitySystem(Arena* arena)
{
    EntityRegistry* registryMem = MENEW(arena, EntityRegistry);
    GetEngineCtx()->entityRegistry = registryMem;
    // dummy entity with bad id so we can return it on failure from methods like GetEntity
    registryMem->entMap[U32_INVALID_ID] = {};
    Entity::SetFlag(U32_INVALID_ID, EntityFlags_Invalid, true);
    Entity::SetFlag(U32_INVALID_ID, EntityFlags_DISABLED, true);
}

void ReinitializeEntitySystem()
{
	EngineContext* ctx = GetEngineCtx();
    ctx->entityRegistry = nullptr;
	Entity::InitializeEntitySystem(&ctx->engineSceneAllocator);
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
    StringView name, 
    const meTransform& tf, 
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
		ent.name = name;
        entityID = HashBytes((u8*)name.data, name.len);
    }
    else
    {
        // if no name, the entity id is just a incrementally increasing num
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

} // namespace Entity