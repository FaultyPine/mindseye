#pragma once

#include "core/me_core.h"
#include "core/containers/me_blocklist.h"

template <typename T>
struct meResourceSlot
{
	T obj = {};
	u16 generation = 0;
	bool inUse = false;
};

struct meResourcePoolBase
{
	virtual Eye Load() { UNIMPLEMENTED(); return {}; }
	virtual void* GetOpaque(Eye handle) const { return nullptr; };
	meAllocator* GetPayloadAllocator() const { return resourcePayloadAllocator; }

	virtual Eye CreateInternal() = 0;
	virtual void DestroyInternal(Eye handle) = 0;

	meAllocator* resourcePayloadAllocator = nullptr;
};

// TODO: each resource should have some kind of meResourceUniqueIdentifier
// that uniquely identifies *that* resource apart from the others. I.E. a hash of it's content
// each resource type should implement their own, so when someone requests 
// "i need a new resource for [this] content", we can somehow dedeuplicate, and give back
// an existing resource handle and increment its ref count
template <typename ResourceType>
struct meResourcePool : public meResourcePoolBase
{
	// NOTE: meResourcePool should be assumed to have pointer stability to its resources
	// There's no hard dependence on that rn, since everything uses handles to reference these, but it's still a nice thing
	// but other than that, there's no constraints on what data structure is used here. Maybe this should be a map
	meBlockList<meResourceSlot<ResourceType>> resourcePool;
	ResourceType badData = {};

	meResourcePool(meAllocator* resourceAllocator, meAllocator* payloadAllocator);
	meResourcePool(const meResourcePool& other) = default;
	meResourcePool(const meResourcePool&& other) = default;
	bool Empty() const { return resourcePool.empty(); }
	// derived resource pools will overload this Load function with
	// their own signature. Those derived functions should use CreateInternal
	// to return & write to the handle
	virtual Eye Load() override { return CreateInternal(); }
	void Destroy(Eye handle) { DestroyInternal(handle); }
	ResourceType& Get(Eye handle);
	const ResourceType& Get(Eye handle) const;
	virtual void* GetOpaque(Eye handle) const override
	{
		return (void*)&Get(handle);
	}

	// each resource pool should assign a default "no data" object
	// in it's constructor
	// EX: a 1x1 white texture
	ResourceType& GetBadData() { return badData; }

	Eye CreateInternal() override;
	void DestroyInternal(Eye handle) override;
};


template <typename ResourceType>
meResourcePool<ResourceType>::meResourcePool(
	meAllocator* resourceAllocator,
	meAllocator* payloadAllocator)
{
	resourcePool = meBlockList<meResourceSlot<ResourceType>>(resourceAllocator);
	resourcePayloadAllocator = payloadAllocator;
}

template <typename ResourceType>
Eye meResourcePool<ResourceType>::CreateInternal()
{
	meResourceSlot<ResourceType> newResourceInstance = {};
	u32 resourceIdx = resourcePool.push(newResourceInstance);
	meResourceSlot<ResourceType>& resource = resourcePool.get(resourceIdx);
	return Eye(resourceIdx, resource.generation);
}

template <typename ResourceType>
void meResourcePool<ResourceType>::DestroyInternal(Eye handle)
{
	auto idx = handle.GetIndex();
	meResourceSlot<ResourceType>& resource = resourcePool.get(idx);
	resource.obj.~ResourceType();
	resource.generation++;
	resourcePool.markDeleted(idx);
}

template <typename ResourceType>
ResourceType& meResourcePool<ResourceType>::Get(Eye handle)
{
	if (!handle)
	{
		return GetBadData();
	}
	auto idx = handle.GetIndex();
	meResourceSlot<ResourceType>& resource = resourcePool.get(idx);
	ME_ASSERT(resource.generation == handle.GetGeneration());
	return resource.obj;
}

template <typename ResourceType>
const ResourceType& meResourcePool<ResourceType>::Get(Eye handle) const
{
	if (!handle)
	{
		return const_cast<meResourcePool<ResourceType>*>(this)->GetBadData();
	}
	auto idx = handle.GetIndex();
	const meResourceSlot<ResourceType>& resource = resourcePool.get(idx);
	ME_ASSERT(resource.generation == handle.GetGeneration());
	return resource.obj;
}