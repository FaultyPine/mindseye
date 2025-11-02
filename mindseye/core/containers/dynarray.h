#ifndef ME_DYNARRAY_H
#define ME_DYNARRAY_H

#include "core/me_defines.h"
#include "core/me_memory.h"

// "stretchy buffer" implementation
// dynamic array that resizes itself when capacity is reached
// stores capacity/size in a header section stored *before* the actual array pointer

#define DynArray_Foreach(array, iteratorVarName) u32 iteratorVarName = 0; iteratorVarName < DynArrayGetSize(array); iteratorVarName++

typedef void* DynArray;
#define DynArray(type) type*

// Frees backing memory
void DynArrayDestroy(DynArray& array);

// Retrives the size from the DynArray header
u32 DynArrayGetSize(DynArray array);
template<typename T>
u32 DynArrayGetSize(T* array)
{
	return DynArrayGetSize((DynArray)array);
}
// Retrives the capacity from the DynArray header
u32 DynArrayGetCapacity(DynArray array);
template <typename T>
u32 DynArrayGetCapacity(T* array)
{
	return DynArrayGetCapacity((DynArray)array);
}
// Retrives the stride from the DynArray header
u32 DynArrayGetStride(DynArray array);
template <typename T>
u32 DynArrayGetStride(T* array)
{
	return DynArrayGetStride((DynArray)array);
}
meAllocator* DynArrayGetAllocator(DynArray array);
template <typename T>
inline T& DynArrayGet(DynArray array, u32 index)
{
    return ((T*)array)[index];
}

// internal
DynArray __DynArrayCreate(u32 stride, u32 initialCapacity, meAllocator* allocator);

// Create an array with an optional initial capacity (number of elements)
template<typename T>
T* DynArrayCreate(meAllocator* allocator, u32 initialCapacity = 5)
{
    return (T*)__DynArrayCreate(sizeof(T), initialCapacity, allocator);
}
// Create an array with an optional initial capacity (number of elements)
template<typename T>
T* DynArrayCreate(meAllocator* allocator, u32 initialSize, T* initialData)
{
    T* result = (T*)__DynArrayCreate(sizeof(T), initialSize, allocator);
    ME_MEMCPY(result, initialData, sizeof(T)*initialSize);
    GetHeaderPointer()->size = initialSize;
    return result;
}

void __DynArrayDestroy(DynArray& array);
#define DynArrayDestroy(dynarray) __DynArrayDestroy((void*)&dynarray)

struct DynArrayHeader
{
    // number of elements currently in the array
    u32 size;
    // number of *elements* we can hold in our backing memory
    u32 capacity;
    // size in bytes of each element
    u32 stride;
    meAllocator* allocator;
};
DynArrayHeader* GetHeaderPointer(DynArray array);

template<typename T>
struct DynArrayScoped
{
	DynArray(T) arr;
	DynArrayScoped(meAllocator* allocator, u32 initialCapacity = 5)
	{
		arr = DynArrayCreate<T>(allocator, initialCapacity);
	}
	~DynArrayScoped()
	{
		DynArrayDestroy(arr);
	}
};

void* __DynArrayPushAt(DynArray array, void* obj, u32 numObjs, u32 index);
// Copies an object to a specified index (and moves all other elements over)
// passing reference as this could potentially reallocate if backing mem is full
// pushing to an index outside the range [0,length] returns nullptr, logs an error, and does nothing
template <typename T>
void DynArrayPushAt(T*& array, T obj, u32 index)
{
    array = (T*)__DynArrayPushAt(array, &obj, 1, index);
}
// Copies an object to the end of the array
template <typename T>
void DynArrayPush(T*& array, T obj)
{
    array = (T*)__DynArrayPushAt((DynArray)array, (void*)&obj, 1, DynArrayGetSize(array));
}

template<typename T>
void DynArrayPush(T*& array, T* objs, u64 numObjs)
{
	array = (T*)__DynArrayPushAt((DynArray)array, (void*)objs, numObjs, DynArrayGetSize(array));
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


MEAPI void DynArrayTests();

#endif
