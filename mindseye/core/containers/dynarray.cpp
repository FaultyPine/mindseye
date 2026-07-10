#pragma once
#include "dynarray.h"


#include "core/me_log.h"
#include "core/me_memory.h"

#define ARRAY_CHECKS (1)

// dynarray size shouldn't change with different types
STATIC_ASSERT(sizeof(DynArray<int>) == sizeof(DynArrayAny));
STATIC_ASSERT(sizeof(DynArrayAny) == sizeof(DynArray<u64>));

template<typename T>
DynArrayHeader* GetHeaderPointer(const DynArray<T>& array)
{
    return const_cast<DynArrayHeader*>(&array.header);
}

// ===== Create & Destroy =====

Allocation DynArrayInternalAlloc(meAllocator* allocator, size_t size)
{
    return MEALLOC(allocator, size);
}

void DynArrayInternalFree(meAllocator* allocator, Allocation data)
{
    allocator->meFree(data);
}

template<typename T>
DynArray<T> DynArrayCreate(
	meAllocator* allocator,
	u32 initialCapacity,
	u32 strideOverride)
{
	u32 stride = strideOverride;
    u32 arraySize = initialCapacity * stride;
    Allocation arrayBackingAlloc = DynArrayInternalAlloc(allocator, arraySize);
    u8* arrayBackingMem = (u8*)arrayBackingAlloc.data;
    ME_MEMCLEAR(arrayBackingMem, arraySize);
    DynArray<T> result;
    result.header.size = 0;
    result.header.capacity = initialCapacity;
    result.header.stride = stride;
    result.header.allocator = allocator;
    result.data = (T*)arrayBackingMem;
    return result;
}

template<typename T>
void DynArrayDestroy(DynArray<T>& array)
{
    if (!array.data) return;
    DynArrayInternalFree(array.header.allocator, Allocation(array.data, array.header.size));
	array = {};
}

template<typename T>
DynArray<T> DynArrayResize(DynArray<T> array, u32 newCapacity)
{
    DynArrayHeader* header = GetHeaderPointer(array);
#if ARRAY_CHECKS
    ME_ASSERT(header->capacity != 0 && "resize called on array with 0 capacity");
#endif
    DynArray<T> newArray = DynArrayCreate<T>(header->allocator, newCapacity, header->stride);
    newArray.header.size = header->size;
    ME_MEMCPY((void*)newArray.data, array.data, header->size * header->stride);
    DynArrayDestroy(array);
    return newArray;
}

// ===== Modify array ======

template <typename T>
bool DynArrayPushAt(
	DynArray<T>& array, 
	T* objs, 
	u32 numObjs, 
	u32 index)
{
    DynArrayHeader* header = GetHeaderPointer(array);
#if ARRAY_CHECKS
    if (index > header->size)
    {
        LOG_FATAL("Tried to push element outside the bounds of a dynarray. index = %u  arr size = %u", index, header->size);
        return false;
    }
#endif
    while (header->size + numObjs > header->capacity)
    {
		constexpr static u32 DYNARRAY_GROWTH_FACTOR = 2;
		u32 newCapacity = header->capacity * DYNARRAY_GROWTH_FACTOR;
		newCapacity = MEMAX(newCapacity, header->size + numObjs);
        array = DynArrayResize(array, newCapacity);
        header = GetHeaderPointer(array);
    }
    u32 arrSize = header->size;
	u32 stride = header->stride;
	T* destination = (T*)((u8*)array.data + (u64)index * stride);
    // if inserting at a populated index, copy all elements to the right
    if (index < arrSize)
    {
		u32 moveCount = arrSize - index;
        T* moveTo = (T*)((u8*)array.data + (u64)(index + numObjs) * stride);
		if constexpr (std::is_trivially_copyable_v<T>)
		{
			u32 moveSize = moveCount * stride;
			ME_MEMMOVE(moveTo, destination, moveSize);
		}
		else
		{
			for (u32 i = moveCount; i >= 0; i--)
			{
				moveTo[i-1] = destination[i-1];
			}
		}
    }
	// copy object(s) to the index
	if constexpr (std::is_trivially_copyable_v<T>)
	{
		ME_MEMCPY(destination, objs, numObjs * stride);
	}
	else
	{
		for (u32 i = 0; i < numObjs; i++)
		{
			destination[i] = objs[i];
		}
	}
    // when pushing to a dynarray with an overridden stride (that != sizeof(T))
    // the "numObjs" being pushed may not be the number of T's
    u32 numAddedObjs = numObjs;
    header->size += numAddedObjs;
    return true;
}

template<typename T>
void __DynArrayPopAt(DynArray<T>& array, u32 index, void* out)
{
    DynArrayHeader* header = GetHeaderPointer(array);
    u32 arrSize = header->size;
    u32 stride = header->stride;
    ME_ASSERT(stride == sizeof(T)); // i don't think we're going to need to pop for an opaque dynarray but if we do, need a similar line as in DynArrayPushAt
#if ARRAY_CHECKS
    if (index >= arrSize)
    {
        LOG_FATAL("Tried to pop at invalid dynarray index. index = %u  arr size = %u", index, arrSize);
        return;
    }
#endif
    u8* arrayMem = (u8*)array.data;
    if (out)
    {
        ME_MEMCPY(out, arrayMem + (index * stride), stride);
    }
    // if not last element, copy everything to the right of it 1 spot to the left
    if (index != arrSize-1)
    {
        u32 moveSize = (arrSize - index) * stride;
        u8* moveTo   = arrayMem + ((index+0) * stride);
        u8* moveFrom = arrayMem + ((index+1) * stride);
        ME_MEMMOVE(moveTo, moveFrom, moveSize);
    }
    header->size--;
}

template<typename T>
void __DynArrayPop(DynArray<T>& array, void* out)
{
    return __DynArrayPopAt(array, DynArrayGetSize(array)-1, out);
}

template<typename T>
void DynArrayClear(DynArray<T>& array)
{
	if (array)
	{
		DynArrayHeader* header = GetHeaderPointer(array);
		header->size = 0;
	}
}

// ===== Get header info ======

template<typename T>
u32 DynArrayGetSize(const DynArray<T>& array)
{
	if (!array)
	{
		return 0;
	}
    const DynArrayHeader* headerPtr = GetHeaderPointer(array);
    return headerPtr->size;
}

template<typename T>
u32 DynArrayGetCapacity(const DynArray<T>& array)
{
	if (!array)
	{
		return 0;
	}
    DynArrayHeader* headerPtr = GetHeaderPointer(array);
    return headerPtr->capacity;
}

template<typename T>
u32 DynArrayGetStride(const DynArray<T>& array)
{
	if (!array)
	{
		return 0;
	}
    DynArrayHeader* headerPtr = GetHeaderPointer(array);
    return headerPtr->stride;
}

template<typename T>
meAllocator* DynArrayGetAllocator(const DynArray<T>& array)
{
	if (!array)
	{
		return nullptr;
	}
    DynArrayHeader* headerPtr = GetHeaderPointer(array);
    return headerPtr->allocator;
}

void DynArrayTests()
{
    LOG_INFO("Testing DynArray...");
    DynArray<s32> arr = DynArrayCreate<s32>(GetDefaultAllocator());
    s32 x = 1;
    DynArrayPush(arr, x);
    ME_ASSERT(DynArrayGetSize(arr) == 1);
    ME_ASSERT(arr[0] == 1);
    DynArrayPush(arr, -1);
    DynArrayPush(arr, 2);
    ME_ASSERT(DynArrayGetSize(arr) == 3);
        
    s32 expected[3] = {1,-1,2};
    for (int i = 0; i < 3; i++)
    {
        ME_ASSERT(expected[i] == arr[i]);
    }
	s32 p = 90;
    DynArrayPushAt(arr, &p, 1, 1);
    s32 expected_2[4] = {1,90,-1,2};
    for (int i = 0; i < 4; i++)
    {
        ME_ASSERT(expected_2[i] == arr[i]);
        LOG_INFO("%i",arr[i]);
    }
    ME_ASSERT(DynArrayGetCapacity(arr) == DynArrayDefaultCapacity);
    ME_ASSERT(DynArrayGetStride(arr) == sizeof(s32));

    for (int i = 0; i < 20; i++)
    {
        DynArrayPush(arr, i);
    }

    ME_ASSERT(DynArrayGetSize(arr) == 24);
    ME_ASSERT(DynArrayGetCapacity(arr) == 40);

    s32 lastElem;
    DynArrayPop(arr, lastElem);
    ME_ASSERT(DynArrayGetSize(arr) == 23);
    ME_ASSERT(lastElem == 19);
    s32 shouldBeZero;
    DynArrayPopAt(arr, 4, &shouldBeZero);
    ME_ASSERT(shouldBeZero == 0);
    ME_ASSERT(DynArrayGetSize(arr) == 22);

    DynArrayClear(arr);
    ME_ASSERT(DynArrayGetSize(arr) == 0);

    s32 shouldntChange = 12345678;
    LOG_INFO("Expecting two fatal errors here:");
    DynArrayPop(arr, shouldntChange);
    DynArrayPopAt(arr, 0, &shouldntChange);
    ME_ASSERT(shouldntChange == 12345678);

    LOG_INFO("DynArray Tests complete");
}

