#pragma once

#include "core/me_defines.h"

// array that stores a fixed-size number of elements in place
// if we go over the fixed size of this array, we dynamically allocate
// more space for extra elements in addition to the fixed size buffer

template <typename T, u32 fixedSize>
struct HybridArray
{
    MEAPI HybridArray(meAllocator* allocator = nullptr);
    MEAPI bool IsUsingInPlaceMemory() const { return elements == &fixedMem[0] || elements == nullptr; }
    MEAPI HybridArray(const HybridArray&& arr)
    {
        internalSize = arr.internalSize;
        capacity = arr.capacity;
        allocator = arr.allocator;
        if (arr.IsUsingInPlaceMemory())
        {
            ME_MEMCPY(fixedMem, arr.fixedMem, sizeof(T) * internalSize);
            elements = &fixedMem[0];
        }
        else
        {
            elements = (T*)allocator->meAlloc(sizeof(T) * capacity);
            ME_MEMCPY(elements, arr.elements, sizeof(T) * internalSize);
        }
    }
	MEAPI HybridArray(const HybridArray& arr)
    {
        internalSize = arr.internalSize;
        capacity = arr.capacity;
        allocator = arr.allocator;
        if (arr.IsUsingInPlaceMemory())
        {
            ME_MEMCPY(fixedMem, arr.fixedMem, sizeof(T) * internalSize);
            elements = &fixedMem[0];
        }
        else
        {
            elements = (T*)allocator->meAlloc(sizeof(T) * capacity);
            ME_MEMCPY(elements, arr.elements, sizeof(T) * internalSize);
        }
    }
    MEAPI HybridArray& operator=(const HybridArray& arr)
    {
        if (this == &arr) return *this;
        // free existing heap allocation if any
        if (!IsUsingInPlaceMemory())
        {
            allocator->meFree(elements);
        }
        internalSize = arr.internalSize;
        capacity = arr.capacity;
        allocator = arr.allocator;
        if (arr.IsUsingInPlaceMemory())
        {
            ME_MEMCPY(fixedMem, arr.fixedMem, sizeof(T) * internalSize);
            elements = &fixedMem[0];
        }
        else
        {
            elements = (T*)allocator->meAlloc(sizeof(T) * capacity);
            ME_MEMCPY(elements, arr.elements, sizeof(T) * internalSize);
        }
        return *this;
    }
    MEAPI ~HybridArray();

    // adds element to end of array
    MEAPI void push_back(const T& element);
    // inserts element at specified index - pushes elements to the right. Clamps index to within bounds
    MEAPI void insert(const T& element, u32 index);
    // (*DOES NOT CALL DTOR/DELETE*) and places the rightmost element in it's place
    MEAPI T erase_and_fill(u32 index);
    // (*DOES NOT CALL DTOR/DELETE*) and moves elements to the right of it to fill the gap
    MEAPI T erase(u32 index);
    MEAPI T pop();

    // returns element at index
    MEAPI T& at(u32 index);
    MEAPI const T& at(u32 index) const;
    MEAPI u32 size() const;
    // sets size to 0 - does not zero out internal memory or do any deallocation
    MEAPI void clear();
 
    MEAPI inline T* get_elements() { return elements; }

    // this points to the current array of elements.
    // when size < fixedSize, elements points to fixedMem.
    // when size >= fixedSize elements points to a heap-allocated array
    T* elements = nullptr;
    T fixedMem[fixedSize] = {};
    // both in terms of number of elements
    u32 internalSize = 0;
    u32 capacity = 0;
    meAllocator* allocator = nullptr;
};

MEAPI void HybridArrayTests();
