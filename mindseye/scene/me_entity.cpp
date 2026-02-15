#pragma once

#include "me_entity.h"
#include "core/me_core.h"
#include "render/me_mesh.h"

StringView EntityRefSerializerToStringFn(
	const meTypeDescriptor& typeDescriptor,
	SerializeContext ctx)
{
	meSpan data = ctx.data;
	EntityRef* ref = (EntityRef*)data.data;
	EntityData& entity = Entity::GetEntity(*ref);
	if (TEST_BIT(entity.flags, EntityFlags_Invalid))
	{
		return STRING_LIT("INVALID_ENTITY");
	}
	if (TEST_BIT(entity.flags, EntityFlags_NoSer))
	{
		return {};
	}
	meSpan entitySpan = SPAN_FROM(entity);
	SerializeContext entityCtx = ctx;
	entityCtx.data = entitySpan;
	json j = JsonSerializeWithTypeDescriptor(TD_ENTITYDATA, entitySpan.data, ctx.parentType);
	std::string bruh = j.dump(4);
	StringView result = StringView(MEALLOC(ctx.allocator, bruh.size()), bruh.size());
	ME_MEMCPY(result.data, bruh.data(), bruh.size());
	return result;
}

bool EntityRefDeserializerFromStringFn(
	const meTypeDescriptor& typeDescriptor,
	DeserializeContext& ctx)
{
	// we serialize/deserialize EntityRef as if it were EntityData
	EntityData entity = {};
	if (!DeserializeFromTextBlocking(
		TD_ENTITYDATA, ctx.externalDataAllocator, 
		StringView(ctx.inputData, ctx.inputData.size), SPAN_FROM(entity)))
	{
		LOG_ERROR("Failed to deserialize entity");
		return false;
	}
	EntityRef ref = Entity::CreateBlankEntity();
	Entity::GetEntity(ref) = entity;
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

void InitializeEntitySystem(meAllocator* allocator)
{
    EntityRegistry* registryMem = MENEW(allocator, EntityRegistry);
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

EntityRef CreateBlankEntity(
	StringView name)
{
    EntityRegistry& registry = GetRegistry();
    EntityData ent = {};
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
		ent.name = StringFormatTmp("UnnamedEntity%i", entityID);
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