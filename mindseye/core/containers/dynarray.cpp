#pragma once
#include "dynarray.h"


#include "core/me_log.h"
#include "core/me_memory.h"

#define ARRAY_CHECKS (1)

// dynarray size shouldn't change with different types
STATIC_ASSERT(sizeof(DynArray<int>) == sizeof(DynArrayAny));
STATIC_ASSERT(sizeof(DynArrayAny) == sizeof(DynArray<u64>));

StringView DynArraySerializerToStringFn(
	const meTypeDescriptor& typeDescriptor,
	SerializeContext ctx)
{
	const meTypeDescriptor* parentType = ctx.parentType;
	// DynArray is templated, and so requires the parent type to understand the template args, see comment in meTypeDescriptor struct
	ME_ASSERT(parentType);
	meSpanTyped<meTypeDescriptor*> templatedTypes = parentType->templatedTypes;
	ME_ASSERT(templatedTypes);
	ME_ASSERT(templatedTypes.size == 1);
	const meTypeDescriptor* templateArg = templatedTypes[0];
	const meSpan& dynArrayData = ctx.data;
	DynArrayAny& arr = *(DynArrayAny*)dynArrayData.data;
	u32 size = DynArrayGetSize(arr);
	u32 stride = DynArrayGetStride(arr);
	StringBuilder builder(ctx.allocator);
	builder.Append(STRING_LIT("["));
	for (u32 i = 0; i < size; i++)
	{
		meSpan elementSpan = meSpan(&arr[i * stride], templateArg->size);
		SerializeContext elementCtx = ctx;
		elementCtx.data = elementSpan;
		StringView elementStr = templateArg->ToString(elementCtx);
		builder.Append(elementStr);
		if (i < (size - 1))
		{
			builder.Append(STRING_LIT(", "));
		}
	}
	builder.Append(STRING_LIT("]"));
	return builder;
}

bool DynArrayDeserializerFromStringFn(
	const meTypeDescriptor& typeDescriptor,
	DeserializeContext& ctx)
{
	const meTypeDescriptor* parentType = ctx.parentType;
	// DynArray is templated, and so requires the parent type to understand the template args, see comment in meTypeDescriptor struct
	ME_ASSERT(parentType);
	meSpanTyped<meTypeDescriptor*> templatedTypes = parentType->templatedTypes;
	ME_ASSERT(templatedTypes);
	ME_ASSERT(templatedTypes.size == 1);
	const meTypeDescriptor* templateArg = templatedTypes[0];

	// BOOKMARK: this is borked
	StringView str = StringView(ctx.inputData.data, ctx.inputData.size);
	while (StringView element = meDeserializeEatUntilNextElement(str, '[', ']', ','))
	{
		DeserializeContext elementCtx = ctx;
		elementCtx.inputData = element.ToSpan();
		templateArg->FromString(elementCtx);
	}
	
	return true;
}

template<typename T>
DynArrayHeader* GetHeaderPointer(const DynArray<T>& array)
{
    u32 headerSize = sizeof(DynArrayHeader);
    // our header will always be 'behind' our array pointer.
    DynArrayHeader* headerPtr = (DynArrayHeader*)(((u8*)array.data) - headerSize);
    return headerPtr;
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
DynArray<T> DynArrayCreate(meAllocator* allocator, u32 initialCapacity)
{
	u32 stride = sizeof(T);
    u32 headerSize = sizeof(DynArrayHeader);
    u32 arraySize = initialCapacity * stride;
    u32 allocSize = headerSize + arraySize;
    Allocation arrayBackingAlloc = DynArrayInternalAlloc(allocator, allocSize);
    u8* arrayBackingMem = (u8*)arrayBackingAlloc.data;
    ME_MEMCLEAR(arrayBackingMem, allocSize);
    // populate header
    DynArrayHeader* headerPointer = (DynArrayHeader*)arrayBackingMem;
    headerPointer->size = 0;
    headerPointer->capacity = initialCapacity;
    headerPointer->stride = stride;
    headerPointer->allocator = allocator;
	// our DynArray is a pointer to our array elements, and metadata about the array
	// is stored just before that pointer
	DynArray<T> result = { (T*)(arrayBackingMem + headerSize) };
    return result;
}

template<typename T>
void DynArrayDestroy(DynArray<T>& array)
{
    // since header info is stored before the array pointer, move back to the beginning of the allocation to free it
    DynArrayHeader* baseArrayPtr = GetHeaderPointer(array);
    DynArrayInternalFree(baseArrayPtr->allocator, Allocation(baseArrayPtr, baseArrayPtr->size));
	array = {};
}

template<typename T>
DynArray<T> DynArrayResize(DynArray<T> array, u32 newCapacity)
{
    DynArrayHeader* header = GetHeaderPointer(array);
#if ARRAY_CHECKS
    ME_ASSERT(header->capacity != 0 && "resize called on array with 0 capacity");
#endif
    DynArray<T> newArray = DynArrayCreate<T>(header->allocator, newCapacity);
    DynArrayHeader* newArrayBasePtr = GetHeaderPointer(newArray);
    u32 totalOldArraySize = (header->size * header->stride) + sizeof(DynArrayHeader);
    ME_MEMCPY(newArrayBasePtr, header, totalOldArraySize);
    newArrayBasePtr->capacity = newCapacity;
    DynArrayDestroy(array);
    return newArray;
}

// ===== Modify array ======

template <typename T>
bool DynArrayPushAt(DynArray<T>& array, void* objs, u32 numObjs, u32 index)
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
    u8* arrayMem = (u8*)array.data;
	u8* destination = arrayMem + (index * stride);
    // if inserting at a populated index, copy all elements to the right
    if (index < arrSize)
    {
        u32 moveSize = (arrSize - index) * stride;
        u8* moveTo = arrayMem + ((index + numObjs) * stride);
        ME_MEMMOVE(moveTo, destination, moveSize);
    }
    // copy object(s) to the index
    ME_MEMCPY(destination, objs, numObjs * stride);
    header->size += numObjs;
    return true;
}

template<typename T>
void __DynArrayPopAt(DynArray<T>& array, u32 index, void* out)
{
    DynArrayHeader* header = GetHeaderPointer(array);
    u32 arrSize = header->size;
    u32 stride = header->stride;
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
    DynArray<s32> arr = DynArrayCreate<s32>(GetSystemAllocator());
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
    DynArrayPushAt(arr, 90, 1);
    s32 expected_2[4] = {1,90,-1,2};
    for (int i = 0; i < 4; i++)
    {
        ME_ASSERT(expected_2[i] == arr[i]);
        LOG_INFO("%i",arr[i]);
    }
    ME_ASSERT(DynArrayGetCapacity(arr) == 5);
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
    DynArrayPopAt(arr, 4, shouldBeZero);
    ME_ASSERT(shouldBeZero == 0);
    ME_ASSERT(DynArrayGetSize(arr) == 22);

    DynArrayClear(arr);
    ME_ASSERT(DynArrayGetSize(arr) == 0);

    s32 shouldntChange = 12345678;
    LOG_INFO("Expecting two fatal errors here:");
    DynArrayPop(arr, shouldntChange);
    DynArrayPopAt(arr, 0, shouldntChange);
    ME_ASSERT(shouldntChange == 12345678);

    LOG_INFO("DynArray Tests complete");
}

