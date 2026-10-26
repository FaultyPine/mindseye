#pragma once

#include "core/me_core.h"
#include "core/containers/me_blocklist.h"
#include "core/me_ptrs.h"

template <typename T>
struct meResourceSlot
{
	meSharedPtr<T> obj = {};
	u16 generation = 0;
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
    meResourceType_TemplateAsset,
    // an "instance" of a template asset from disk
    // I.E. a Scene instance might have all the same data as the template asset, 
    // but the positions of all the entities in the scene are different from the initial values on disk
    // runtime-only data is also populated in these - like a mesh GPU buffer handle
    meResourceType_InstanceAsset,
};
// we also track the resource type in Eye, with only 1 bit. So we can't have more than 2 resource types.
static_assert(meResourceType_InstanceAsset == 1);

struct meResourceCreateParams
{
    meResourceType resourceType;
};

struct meResourcePoolBase
{
	virtual Eye Load(meResourceCreateParams createParams) { UNIMPLEMENTED(); return {}; }
	virtual meSharedPtr<void> GetShared(Eye handle) const { return {}; }
	meAllocator* GetPayloadAllocator() const { return resourcePayloadAllocator; }

	virtual Eye Create(meResourceCreateParams createParams) = 0;
	virtual void Destroy(Eye handle) = 0;
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
	ResourcePoolType resourcePoolTemplates;
	ResourcePoolType resourcePoolInstances;

	meResourcePool(
        meAllocator* resourceAllocator, 
        meAllocator* payloadAllocator);
	meResourcePool(const meResourcePool& other) = default;
	meResourcePool(const meResourcePool&& other) = default;
    ~meResourcePool() override
    {
        Clear();
    }
    void Clear();
	bool Empty() const { return resourcePoolTemplates.empty() && resourcePoolInstances.empty(); }
	// derived resource pools will overload this Load function with
	// their own signature. Those derived functions should use Create
	// to return & write to the handle
	virtual Eye Load(meResourceCreateParams createParams) override { return Create(createParams); }
	ResourceType& Get(Eye handle);
	const ResourceType& Get(Eye handle) const;
	meSharedPtr<void> GetShared(Eye handle) const override;

	// each resource pool should assign a default "no data" object
	// in it's constructor
	// EX: a 1x1 white texture
    // if it isn't assigned, the "bad data" is just a default-constructed resource
	ResourceType& GetBadData() { return Get(EYE_INVALID); }

	Eye Create(meResourceCreateParams createParams) override;
	void Destroy(Eye handle) override;

private:
	meSharedPtr<ResourceType> CreateResource();

    ResourcePoolType& GetAssociatedResourcePool(Eye eye)
    {
        return eye.IsTemplateAsset() ? resourcePoolTemplates : resourcePoolInstances;
    }
    ResourcePoolType& GetAssociatedResourcePool(const meResourceCreateParams& createParams)
    {
        if (createParams.resourceType == meResourceType_TemplateAsset)
        {
            return resourcePoolTemplates;
        }
        ME_ASSERT(createParams.resourceType == meResourceType_InstanceAsset)
        return resourcePoolInstances;
    }

    const ResourcePoolType& GetAssociatedResourcePool(Eye eye) const
    {
        return eye.IsTemplateAsset() ? resourcePoolTemplates : resourcePoolInstances;
    }
    const ResourcePoolType& GetAssociatedResourcePool(const meResourceCreateParams& createParams) const
    {
        if (createParams.resourceType == meResourceType_TemplateAsset)
        {
            return resourcePoolTemplates;
        }
        ME_ASSERT(createParams.resourceType == meResourceType_InstanceAsset)
        return resourcePoolInstances;
    }
};

template <typename ResourceType>
meResourcePool<ResourceType>::meResourcePool(
	meAllocator* resourceAllocator,
	meAllocator* payloadAllocator)
{
	resourcePoolTemplates = ResourcePoolType(resourceAllocator);
	resourcePoolInstances = ResourcePoolType(resourceAllocator);
	resourcePayloadAllocator = payloadAllocator;
    // push bad data (default constructed resource) as the first thing in all pools
    // this way, an Eye of "0" is "valid", in that we don't need to assert, we can gracefully handle it elsewhere
    for (u32 i = 0; i <= 1; i++)
    {
        ResourcePoolType& resourcePool = GetAssociatedResourcePool({ .resourceType = (meResourceType)i });
        meResourceSlot<ResourceType> badData = {};
        badData.obj = CreateResource();
        badData.inUse = true;
        u32 idx = resourcePool.push(meMove(badData)); 
        UNUSED(idx);
        ME_ASSERT(idx == 0);
    }
}

template <typename ResourceType>
Eye meResourcePool<ResourceType>::Create(meResourceCreateParams createParams)
{
    ResourcePoolType& resourcePool = GetAssociatedResourcePool(createParams);
	meResourceSlot<ResourceType> newResourceInstance = {};
	u32 resourceIdx = resourcePool.push(meMove(newResourceInstance));
    ME_ASSERT(resourceIdx != 0);
	meResourceSlot<ResourceType>& resource = resourcePool.get(resourceIdx);
	resource.obj = CreateResource();
	return Eye(resourceIdx, resource.generation, createParams.resourceType == meResourceType_TemplateAsset);
}

template <typename ResourceType>
void meResourcePool<ResourceType>::Clear()
{
    for (u32 i = 0; i <= 1; i++)
    {
        ResourcePoolType& resourcePool = GetAssociatedResourcePool({ .resourceType = (meResourceType)i });
        for (auto& res : resourcePool)
        {
            res.obj.reset();
        }
        resourcePool.clear();
    }
    ME_ASSERT(resourcePoolTemplates.allocator == resourcePoolInstances.allocator);
    new (this) meResourcePool<ResourceType>(resourcePoolTemplates.allocator, resourcePayloadAllocator);
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
void meResourcePool<ResourceType>::Destroy(Eye handle)
{
	if (handle.GetIndex() == 0)
	{
		return;
	}
    ResourcePoolType& resourcePool = GetAssociatedResourcePool(handle);
	auto idx = handle.GetIndex();
	meResourceSlot<ResourceType>& resource = resourcePool.get(idx);
	resource.obj.reset();
	resource.generation++;
	resource.inUse = false;
	resourcePool.markDeleted(idx);
}

template <typename ResourceType>
ResourceType& meResourcePool<ResourceType>::Get(Eye handle)
{
    ResourcePoolType& resourcePool = GetAssociatedResourcePool(handle);
	auto idx = handle.GetIndex();
	meResourceSlot<ResourceType>& resource = resourcePool.get(idx);
    u32 generation = handle.GetGeneration();
	ME_ASSERT(resource.generation == generation);
	return *resource.obj;
}

template <typename ResourceType>
const ResourceType& meResourcePool<ResourceType>::Get(Eye handle) const
{
    const ResourcePoolType& resourcePool = GetAssociatedResourcePool(handle);
	auto idx = handle.GetIndex();
	const meResourceSlot<ResourceType>& resource = resourcePool.get(idx);
	ME_ASSERT(resource.generation == handle.GetGeneration());
	return *resource.obj;
}

template <typename ResourceType>
meSharedPtr<void> meResourcePool<ResourceType>::GetShared(Eye handle) const
{
	const ResourcePoolType& resourcePool = GetAssociatedResourcePool(handle);
	const meResourceSlot<ResourceType>& resource = resourcePool.get(handle.GetIndex());
	ME_ASSERT(resource.generation == handle.GetGeneration());
	return resource.obj;
}
