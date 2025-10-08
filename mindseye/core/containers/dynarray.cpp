#include "dynarray.h"


#include "core/me_log.h"
#include "core/me_memory.h"

#define ARRAY_CHECKS 1



DynArrayHeader* GetHeaderPointer(DynArray array)
{
    u32 headerSize = sizeof(DynArrayHeader);
    // our header will always be 'behind' our array pointer.
    DynArrayHeader* headerPtr = (DynArrayHeader*)(((u8*)array) - headerSize);
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

DynArray __DynArrayCreate(u32 stride, u32 initialCapacity, meAllocator* allocator)
{
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
    DynArray result = arrayBackingMem + headerSize; 
    return result;
}

void __DynArrayDestroy(void* dynArrayPtr)
{
	DynArray& array = *(DynArray*)dynArrayPtr;
    // since header info is stored before the array pointer, move back to the beginning of the allocation to free it
    DynArrayHeader* baseArrayPtr = GetHeaderPointer(array);
    DynArrayInternalFree(baseArrayPtr->allocator, Allocation(baseArrayPtr, baseArrayPtr->size));
	array = (void*)0;
}

DynArray DynArrayResize(DynArray array, u32 newCapacity)
{
    DynArrayHeader* header = GetHeaderPointer(array);
#if ARRAY_CHECKS
    ME_ASSERT(header->capacity != 0 && "resize called on array with 0 capacity");
#endif
    DynArray newArray = __DynArrayCreate(header->stride, newCapacity, header->allocator);
    DynArrayHeader* newArrayBasePtr = GetHeaderPointer(newArray);
    u32 totalOldArraySize = (header->size * header->stride) + sizeof(DynArrayHeader);
    ME_MEMCPY(newArrayBasePtr, header, totalOldArraySize);
    newArrayBasePtr->capacity = newCapacity;
    DynArrayDestroy(array);
    return newArray;
}

// ===== Modify array ======

void* __DynArrayPushAt(DynArray array, void* objs, u32 numObjs, u32 index)
{
    DynArrayHeader* header = GetHeaderPointer(array);
#if ARRAY_CHECKS
    if (index > header->size)
    {
        LOG_FATAL("Tried to push element outside the bounds of a dynarray. index = %u  arr size = %u", index, header->size);
        return nullptr;
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
    u8* arrayMem = (u8*)array;
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
    return array;
}

void __DynArrayPopAt(DynArray array, u32 index, void* out)
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
    u8* arrayMem = (u8*)array;
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

void __DynArrayPop(DynArray array, void* out)
{
    return __DynArrayPopAt(array, DynArrayGetSize(array)-1, out);
}

void DynArrayClear(DynArray array)
{
    DynArrayHeader* header = GetHeaderPointer(array);
    header->size = 0;
}

// ===== Get header info ======

u32 DynArrayGetSize(DynArray array)
{
    DynArrayHeader* headerPtr = GetHeaderPointer(array);
    return headerPtr->size;
}

u32 DynArrayGetCapacity(DynArray array)
{
    DynArrayHeader* headerPtr = GetHeaderPointer(array);
    return headerPtr->capacity;
}

u32 DynArrayGetStride(DynArray array)
{
    DynArrayHeader* headerPtr = GetHeaderPointer(array);
    return headerPtr->stride;
}

meAllocator* DynArrayGetAllocator(DynArray array)
{
    DynArrayHeader* headerPtr = GetHeaderPointer(array);
    return headerPtr->allocator;
}

void DynArrayTests()
{
    LOG_INFO("Testing DynArray...");
    s32* arr = DynArrayCreate<s32>(GetSystemAllocator());
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

    u32 shouldntChange = 0xDEADBEEF;
    LOG_INFO("Expecting two fatal errors here:");
    DynArrayPop(arr, shouldntChange);
    DynArrayPopAt(arr, 0, shouldntChange);
    ME_ASSERT(shouldntChange == 0xDEADBEEF);

    LOG_INFO("DynArray Tests complete");
}

