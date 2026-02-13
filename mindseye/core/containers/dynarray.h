#ifndef ME_DYNARRAY_H
#define ME_DYNARRAY_H

#include "core/me_defines.h"
#include "core/me_memory.h"
#include "reflector/reflection_types.h"

// "stretchy buffer" implementation
// dynamic array that resizes itself when capacity is reached
// stores capacity/size in a header section stored *before* the actual array pointer

#define DynArray_Foreach(array, iteratorVarName) u32 iteratorVarName = 0; !!(array) && iteratorVarName < DynArrayGetSize(array); iteratorVarName++

// TODO: maybe some magic at the beginning would be a good idea
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

template <typename T>
struct DynArray
{
	T* data = nullptr;

	operator bool() const
	{
		return data != nullptr;
	}
	T& operator[](u32 index)
	{
		return data[index];
	}
	const T & operator[](u32 index) const
	{
		return data[index];
	}
	operator T*()
	{
		return data;
	}
};
typedef DynArray<u8> DynArrayAny;

StringView DynArraySerializerToStringFn(
	const meTypeDescriptor& typeDescriptor,
	SerializeContext ctx);

bool DynArrayDeserializerFromStringFn(
	const meTypeDescriptor& typeDescriptor,
	DeserializeContext& ctx);

// Create an array with an optional initial capacity (number of elements)
template<typename T>
DynArray<T> DynArrayCreate(meAllocator* allocator, u32 initialCapacity = 10);

// Frees backing memory
template <typename T>
void DynArrayDestroy(DynArray<T>& array);

// ================================= DynArray modifiers =================================

// Copies an object to a specified index (and moves all other elements over)
// passing reference as this could potentially reallocate if backing mem is full
// pushing to an index outside the range [0,length] returns nullptr, logs an error, and does nothing
template <typename T>
void DynArrayPushAt(DynArray<T>& array, T obj, u32 index)
{
    DynArrayPushAt(array, &obj, 1, index);
}
// Copies an object to the end of the array
template <typename T>
void DynArrayPush(DynArray<T>& array, T obj)
{
    DynArrayPushAt(array, &obj, 1, DynArrayGetSize(array));
}

template<typename T>
void DynArrayPush(DynArray<T>& array, T* objs, u64 numObjs)
{
	DynArrayPushAt(array, objs, numObjs, DynArrayGetSize(array));
}

// remove (and optionally return element) at specified index
// popping at an index outside the range [0,length-1] does nothing and logs an error
template <typename T>
inline void DynArrayPopAt(DynArray<T>& array, u32 index, T& out)
{
    __DynArrayPopAt(array, index, &out);
}

// remove (and optionally return) the last element
template <typename T>
inline void DynArrayPop(DynArray<T>& array, T& out)
{
    __DynArrayPop<T>(array, &out);
}

// sets array size to 0, does not free backing memory
template <typename T>
void DynArrayClear(DynArray<T>& array);


// ================================= DynArray accessors =================================

// Retrives the size from the DynArray header
template <typename T>
u32 DynArrayGetSize(const DynArray<T>& array);

// Retrives the capacity from the DynArray header
template <typename T>
u32 DynArrayGetCapacity(const DynArray<T>& array);

// Retrives the stride from the DynArray header
template <typename T>
u32 DynArrayGetStride(const DynArray<T>& array);

template <typename T>
meAllocator* DynArrayGetAllocator(DynArray<T>& array);

template <typename T>
inline T& DynArrayGet(DynArray<T>& array, u32 index)
{
    return array[index];
}

// ==========================================================================

template<typename T>
struct DynArrayScoped
{
	DynArray<T> arr;
	DynArrayScoped(meAllocator* allocator, u32 initialCapacity = 5)
	{
		arr = DynArrayCreate<T>(allocator, initialCapacity);
	}
	~DynArrayScoped()
	{
		DynArrayDestroy(arr);
	}
};

void DynArrayTests();


// ugh templates....
#include "dynarray.cpp"


#endif
