#pragma once

#include "core/me_core.h"
#include "core/containers/me_map.h"
#include "asset/me_asset.h"

template <typename T>
struct meResourceSlot
{
	T obj = {};
	u16 generation = 0;
	bool inUse = false;

	operator T&() { return obj; }
};
// TODO: each resource should have some kind of meResourceUniqueIdentifier
// that uniquely identifies *that* resource apart from the others. I.E. a hash of it's content
// each resource type should implement their own, so when someone requests 
// "i need a new resource for [this] content", we can somehow dedeuplicate, and give back
// an existing resource handle and increment its ref count
// Edit: maybe best to have this done by calling into the asset system to get the source hash, or maybe we should store the assetident as keys
template <typename ResourceType, typename Derived>
struct meResourcePool
{
	// NOTE: meResourcePool should be assumed to have pointer stability to its resources
	// There's no hard dependence on that rn, since everything uses handles to reference these, but it's still a nice thing
	// but other than that, there's no constraints on what data structure is used here. Maybe this should be a map
	meMap<MAID, meResourceSlot<ResourceType>> resourcePool;
	meAllocator* resourcePayloadAllocator = nullptr;
	ResourceType badData = {};

	meResourcePool(meAllocator* resourceAllocator, meAllocator* payloadAllocator);
	meResourcePool(const meResourcePool& other) = default;
	meResourcePool(const meResourcePool&& other) = default;
	bool Empty() const { return resourcePool.empty(); }
	// derived resource pools will overload this Load function with
	// their own signature. Those derived functions should use CreateInternal
	// to return & write to the handle
	MAID Load() { return CreateInternal(); }
	void Destroy(MAID handle) { DestroyInternal(handle); }
	ResourceType& Get(MAID handle);
	const ResourceType& Get(MAID handle) const;
	meAllocator* GetPayloadAllocator() const { return resourcePayloadAllocator; }
	MAID CreateIdent()
	{
		meAssetType type = static_cast<Derived*>(this)->GetAssetType();
		MAID newmaid = MAID(resourceUniqueID++, type);
		return newmaid;
	}
	// overridden by derived
	meAssetType GetAssetType() const
	{
		return MABadData;
	}

	// each resource pool should assign a default "no data" object
	// in it's constructor
	// EX: a 1x1 white texture
	ResourceType& GetBadData() { return badData; }

	// these shouldn't get overridden
	MAID CreateInternal();
	void DestroyInternal(MAID handle);

	u32 resourceUniqueID = 0;
};


template <typename ResourceType, typename Derived>
meResourcePool<ResourceType, Derived>::meResourcePool(
	meAllocator* resourceAllocator,
	meAllocator* payloadAllocator)
{
	//resourcePool = meMap<MAID, meResourceSlot<ResourceType>>(resourceAllocator);
	resourcePool = meMap<MAID, meResourceSlot<ResourceType>>();
	resourcePayloadAllocator = payloadAllocator;
}

template <typename ResourceType, typename Derived>
MAID meResourcePool<ResourceType, Derived>::CreateInternal()
{
	MAID newmaid = CreateIdent();
	meResourceSlot<ResourceType>& slot = resourcePool[newmaid];
	ME_ASSERT(!slot.inUse);
	slot.inUse = true;
	return newmaid;
}

template <typename ResourceType, typename Derived>
void meResourcePool<ResourceType, Derived>::DestroyInternal(MAID handle)
{
	resourcePool.erase(handle);
}

template <typename ResourceType, typename Derived>
ResourceType& meResourcePool<ResourceType, Derived>::Get(MAID handle)
{
	auto it = resourcePool.find(handle);
	if (handle && it != resourcePool.end())
	{
		//ME_ASSERT(resource.generation == handle.GetGeneration());
		meResourceSlot<ResourceType>& slot = it->second;
		ME_ASSERT(slot.inUse);
		return slot.obj;
	}
	return const_cast<meResourcePool<ResourceType, Derived>*>(this)->GetBadData();
}

template <typename ResourceType, typename Derived>
const ResourceType& meResourcePool<ResourceType, Derived>::Get(MAID handle) const
{
	auto it = resourcePool.find(handle);
	if (handle && it != resourcePool.end())
	{
		//ME_ASSERT(resource.generation == handle.GetGeneration());
		const meResourceSlot<ResourceType>& slot = it->second;
		ME_ASSERT(slot.inUse);
		return slot.obj;
	}
	return const_cast<meResourcePool<ResourceType, Derived>*>(this)->GetBadData();
}
