#ifndef ME_DYNARRAY_H
#define ME_DYNARRAY_H

#include "core/me_defines.h"
#include "core/me_memory.h"

// "stretchy buffer" implementation
// dynamic array that resizes itself when capacity is reached
// stores capacity/size in a header section stored *before* the actual array pointer

typedef void* DynArray;

// Create an array with an optional initial capacity (number of elements)
DynArray __DynArrayCreate(u32 stride, u32 initialCapacity, meAllocator* allocator);
template<typename T>
T* DynArrayCreate(meAllocator* allocator, u32 initialCapacity = 5)
{
    return (T*)__DynArrayCreate(sizeof(T), initialCapacity, allocator);
}

// Frees backing memory
void DynArrayDestroy(DynArray& array);

// Retrives the size from the DynArray header
u32 DynArrayGetSize(DynArray array);
// Retrives the capacity from the DynArray header
u32 DynArrayGetCapacity(DynArray array);
// Retrives the stride from the DynArray header
u32 DynArrayGetStride(DynArray array);
meAllocator* DynArrayGetAllocator(DynArray array);

template <typename T>
inline T& DynArrayGet(DynArray array, u32 index)
{
    return ((T*)array)[index];
}

void* __DynArrayPushAt(DynArray array, void* obj, u32 index);
// Copies an object to a specified index (and moves all other elements over)
// passing reference as this could potentially reallocate if backing mem is full
// pushing to an index outside the range [0,length] returns nullptr, logs an error, and does nothing
template <typename T>
void DynArrayPushAt(T*& array, T obj, u32 index)
{
    array = (T*)__DynArrayPushAt(array, &obj, index);
}
inline void* __DynArrayPush(DynArray array, void* obj);
// Copies an object to the end of the array
template <typename T>
void DynArrayPush(T*& array, T obj)
{
    array = (T*)__DynArrayPush((DynArray*)array, (void*)&obj);
}

void __DynArrayPopAt(DynArray array, u32 index, void* out = 0);
// remove (and optionally return element) at specified index
// popping at an index outside the range [0,length-1] does nothing and logs an error
template <typename T>
inline void DynArrayPopAt(DynArray array, u32 index, T& out)
{
    __DynArrayPopAt(array, index, &out);
}

// remove (and optionally return) the last element
inline void __DynArrayPop(DynArray array, void* out = 0);
template <typename T>
inline void DynArrayPop(DynArray array, T& out)
{
    __DynArrayPop(array, &out);
}

// sets array size to 0, does not free backing memory
void DynArrayClear(DynArray array);


#define DynArrayForEach(arr, idx) \
    int idx = 0; idx < DynArrayGetSize(arr); i++

#endif


MEAPI void DynArrayTests();