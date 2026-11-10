#pragma once

#include "core/me_core.h"
#include "core/containers/me_blocklist.h"
#include "core/me_ptrs.h"

template <typename T>
struct meResourceSlot
{
	meSharedPtr<T> obj = {};
	u8 generation = 0;
	bool inUse = false;

	meResourceSlot& operator=(const meResourceSlot& other)
	{
		obj = other.obj;
		// Preserve the generation when a deleted block-list slot is reused.
		inUse = other.inUse;
		return *this;
	}
};

enum meResourceType : u8
{
    // an asset loaded from disk. Typically read-only
    // I.E. a Scene template asset might have a path to a gltf file and info about what entities 
    // are in the scene at what positions
    meResourceType_TemplateAsset = 0,
    // an "instance" of a template asset from disk
    // I.E. a Scene instance might have all the same data as the template asset, 
    // but the positions of all the entities in the scene are different from the initial values on disk
    // runtime-only data is also populated in these - like a mesh GPU buffer handle
    meResourceType_InstanceAsset,

    meResourceType_ImportedAsset,

    meResourceType_NUM_TYPES,
};

struct meResource
{
    Eye handle = {};
    meResourceType type = meResourceType_TemplateAsset;

    explicit operator bool() const { return static_cast<bool>(handle); }
    bool operator==(const meResource& other) const = default;
};

static const meResource ME_RESOURCE_INVALID = {EYE_INVALID, meResourceType_TemplateAsset};

struct meResourceCreateParams
{
    meResourceType resourceType;
};

struct meResourcePoolBase
{
	virtual meResource Load(meResourceCreateParams createParams) { return Create(createParams); }
	virtual meSharedPtr<void> GetShared(meResource handle) const { return {}; }
	meAllocator* GetPayloadAllocator() const { return resourcePayloadAllocator; }

	virtual meResource Create(meResourceCreateParams createParams) = 0;
	virtual void Destroy(meResource handle) = 0;
    virtual ~meResourcePoolBase() {}

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
    using ResourcePoolType = meBlockList<meResourceSlot<ResourceType>>;
    // TODO: these should be concurrent hashmap
	ResourcePoolType resourcePools[meResourceType_NUM_TYPES];

	meResourcePool(
        meAllocator* resourceAllocator, 
        meAllocator* payloadAllocator);
	meResourcePool(const meResourcePool& other) = default;
	meResourcePool(const meResourcePool&& other) = default;
    ~meResourcePool() override = default;
    void Clear();
	bool Empty() const
    {
        for (const auto& pool : resourcePools)
        {
            if (!pool.empty()) return false;
        }
        return true;
    }

	ResourceType& Get(meResource handle);
	const ResourceType& Get(meResource handle) const;
	meSharedPtr<void> GetShared(meResource handle) const override;

	// each resource pool should assign a default "no data" object
	// in it's constructor
	// EX: a 1x1 white texture
    // if it isn't assigned, the "bad data" is just a default-constructed resource
	ResourceType& GetBadData() { return Get(ME_RESOURCE_INVALID); }

	meResource Create(meResourceCreateParams createParams) override;
	void Destroy(meResource handle) override;

private:
	meSharedPtr<ResourceType> CreateResource();

    ResourcePoolType& GetAssociatedResourcePool(meResource resource)
    {
        ME_ASSERT(resource.type < meResourceType_NUM_TYPES);
        return resourcePools[resource.type];
    }
    ResourcePoolType& GetAssociatedResourcePool(const meResourceCreateParams& createParams)
    {
        ME_ASSERT(createParams.resourceType < meResourceType_NUM_TYPES);
        return resourcePools[createParams.resourceType];
    }

    const ResourcePoolType& GetAssociatedResourcePool(meResource resource) const
    {
        ME_ASSERT(resource.type < meResourceType_NUM_TYPES);
        return resourcePools[resource.type];
    }
};

template <typename ResourceType>
meResourcePool<ResourceType>::meResourcePool(
	meAllocator* resourceAllocator,
	meAllocator* payloadAllocator)
{
    for (auto& pool : resourcePools)
    {
        pool.allocator = resourceAllocator;
    }
    resourcePayloadAllocator = payloadAllocator;
    Clear();
}

template <typename ResourceType>
meResource meResourcePool<ResourceType>::Create(meResourceCreateParams createParams)
{
    ResourcePoolType& resourcePool = GetAssociatedResourcePool(createParams);
	meResourceSlot<ResourceType> newResourceInstance = {};
	u32 resourceIdx = resourcePool.push(meMove(newResourceInstance));
    ME_ASSERT(resourceIdx != 0);
	meResourceSlot<ResourceType>& resource = resourcePool.get(resourceIdx);
	resource.obj = CreateResource();
	resource.inUse = true;
	return {Eye(resourceIdx, resource.generation), createParams.resourceType};
}

template <typename ResourceType>
void meResourcePool<ResourceType>::Clear()
{
    for (auto& resourcePool : resourcePools)
    {
        resourcePool.clear();
        // Reserve index zero for the default resource in every pool.
        meResourceSlot<ResourceType> badData = {};
        badData.obj = CreateResource();
        badData.inUse = true;
        u32 idx = resourcePool.push(badData);
        UNUSED(idx);
        ME_ASSERT(idx == 0);
    }
}

template <typename T>
concept HasDestroyFn = requires(T t)
{
    t.Destroy();
};

template <typename ResourceType>
meSharedPtr<ResourceType> meResourcePool<ResourceType>::CreateResource()
{
	meAllocator* allocator = resourcePayloadAllocator;
	ResourceType* resource = MENEW(allocator, ResourceType);
	return meSharedPtr<ResourceType>(resource, [allocator](ResourceType* ptr)
	{
		if constexpr (HasDestroyFn<ResourceType>)
		{
			ptr->Destroy();
		}
		MEDELETE(allocator, ResourceType, ptr);
	});
}

template <typename ResourceType>
void meResourcePool<ResourceType>::Destroy(meResource handle)
{
	if (handle.handle.GetIndex() == 0)
	{
		return;
	}
    ResourcePoolType& resourcePool = GetAssociatedResourcePool(handle);
	auto idx = handle.handle.GetIndex();
	meResourceSlot<ResourceType>& resource = resourcePool.get(idx);
	resource.obj.reset();
	resource.generation++;
	resource.inUse = false;
	resourcePool.markDeleted(idx);
}

template <typename ResourceType>
ResourceType& meResourcePool<ResourceType>::Get(meResource handle)
{
    ResourcePoolType& resourcePool = GetAssociatedResourcePool(handle);
	auto idx = handle.handle.GetIndex();
	meResourceSlot<ResourceType>& resource = resourcePool.get(idx);
    u32 generation = handle.handle.GetGeneration();
	ME_ASSERT(resource.generation == generation);
	return *resource.obj;
}

template <typename ResourceType>
const ResourceType& meResourcePool<ResourceType>::Get(meResource handle) const
{
    const ResourcePoolType& resourcePool = GetAssociatedResourcePool(handle);
	auto idx = handle.handle.GetIndex();
	const meResourceSlot<ResourceType>& resource = resourcePool.get(idx);
	ME_ASSERT(resource.generation == handle.handle.GetGeneration());
	return *resource.obj;
}

template <typename ResourceType>
meSharedPtr<void> meResourcePool<ResourceType>::GetShared(meResource handle) const
{
	const ResourcePoolType& resourcePool = GetAssociatedResourcePool(handle);
	const meResourceSlot<ResourceType>& resource = resourcePool.get(handle.handle.GetIndex());
	ME_ASSERT(resource.generation == handle.handle.GetGeneration());
	return resource.obj;
}
