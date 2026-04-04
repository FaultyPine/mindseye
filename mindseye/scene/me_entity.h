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
	EntityFlags_NoSer, // runtime-only entity
    EntityFlags_Selected,

    EntityFlags_NUM_ENTITY_FLAGS,
};
STATIC_ASSERT(EntityFlags_NUM_ENTITY_FLAGS < 32);

struct MEREFLECT(type, Serializer=EntityRefSerializerToStringFn, Deserializer=EntityRefDeserializerFromStringFn) 
EntityRef
{
	u32 ref = U32_INVALID_ID;
	EntityRef(u32 r) : ref(r) {}
	EntityRef() = default;
	explicit operator u32() const { return ref; }
    explicit operator bool() const { return ref != U32_INVALID_ID; }
	bool operator==(const EntityRef& other) const
	{
		return ref == other.ref;
	}
};

MEMAP_BEGIN_CUSTOM_HASHER(EntityRef, obj) 
{
    return HashBytesL((u8*)&obj.ref, sizeof(obj.ref));
}
MEMAP_END_CUSTOM_HASHER

struct MEREFLECT(type) EntityData
{
    meTransform transform = {};
    MEREFLECT(tooltip = "The 'appearance' of the entity")
    MAID mesh = MAID::Of<MAMesh>();
	// gameplay-focused bounds. Rendering bounds including anims may be different (stored on mesh)
    BoundingBox authoritativeBounds = {}; 
    EntityFlags flags = 0;
	String name = {};
    
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

void InitializeEntitySystem(meAllocator* allocator);
void DeinitializeEntitySystem();


void EntityRefSerializerToStringFn(
	const meTypeDescriptor& typeDescriptor,
	SerializeContext& ctx);

bool EntityRefDeserializerFromStringFn(
	const meTypeDescriptor& typeDescriptor,
	DeserializeContext& ctx);

MEAPI EntityRef CreateBlankEntity(
	StringView name = {});
MEAPI bool DestroyEntity(EntityRef ent);
MEAPI EntityData& GetEntity(EntityRef ent);

MEAPI void SetFlag(EntityRef ent, EntityFlags flag, bool enabled);
MEAPI void SetFlag(EntityData& ent, EntityFlags flag, bool enabled);
MEAPI bool IsFlag(EntityRef ent, EntityFlags flag);
MEAPI bool IsFlag(const EntityData& ent, EntityFlags flag);

} // namespace Entity

