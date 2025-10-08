#pragma once

#include "core/me_core.h"
#include "core/containers/me_blocklist.h"

template <typename T>
struct meResourceSlot
{
	T obj = {};
	u16 generation = 0;
	bool inUse = false;

	operator T&() { return obj; }
};

template <typename ResourceType>
struct meResourcePool
{
	// NOTE: meResourcePool should be assumed to have pointer stability to its resources
	meBlockList<meResourceSlot<ResourceType>> resourcePool;
	meAllocator* resourcePayloadAllocator = nullptr;

	meResourcePool(meAllocator* resourceAllocator, meAllocator* payloadAllocator);
	meResourcePool(const meResourcePool& other) = default;
	meResourcePool(const meResourcePool&& other) = default;
	bool Empty() const { return resourcePool.empty(); }
	Eye Create() { return CreateInternal(); }
	void Destroy(Eye eye) { DestroyInternal(eye); }
	ResourceType& Get(Eye eye);
	const ResourceType& Get(Eye eye) const;
	meAllocator* GetPayloadAllocator() const { return resourcePayloadAllocator; }

protected:
	Eye CreateInternal();
	void DestroyInternal(Eye eye);
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
	auto& resource = resourcePool.get(resourceIdx);
	return Eye(resourceIdx, resource.generation);
}

template <typename ResourceType>
void meResourcePool<ResourceType>::DestroyInternal(Eye eye)
{
	auto idx = eye.GetIndex();
	auto& resource = resourcePool.get(idx);
	resource.generation++;
	resourcePool.markDeleted(eye);
}

template <typename ResourceType>
ResourceType& meResourcePool<ResourceType>::Get(Eye eye)
{
	auto idx = eye.GetIndex();
	auto& resource = resourcePool.get(idx);
	ME_ASSERT(resource.generation == eye.GetGeneration());
	return resource.obj;
}

template <typename ResourceType>
const ResourceType& meResourcePool<ResourceType>::Get(Eye eye) const
{
	auto idx = eye.GetIndex();
	const auto& resource = resourcePool.get(idx);
	ME_ASSERT(resource.generation == eye.GetGeneration());
	return resource.obj;
}