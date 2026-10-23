#pragma once

#include "core/me_defines.h"
#include "core/me_memory.h"

// array that stores a fixed-size number of elements in place
// if we go over the fixed size of this array, we dynamically allocate
// more space for extra elements in addition to the fixed size buffer

template <typename T, u32 fixedSize>
struct HybridArray
{
    HybridArray(meAllocator* allocator = nullptr);
    bool IsUsingInPlaceMemory() const { return elements == &fixedMem[0] || elements == nullptr; }
    HybridArray(const HybridArray&& arr)
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
	HybridArray(const HybridArray& arr)
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
    HybridArray& operator=(const HybridArray& arr)
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
    ~HybridArray();

    // adds element to end of array
    void push_back(const T& element);
    // inserts element at specified index - pushes elements to the right. Clamps index to within bounds
    void insert(const T& element, u32 index);
    // (*DOES NOT CALL DTOR/DELETE*) and places the rightmost element in it's place
    T erase_and_fill(u32 index);
    // (*DOES NOT CALL DTOR/DELETE*) and moves elements to the right of it to fill the gap
    T erase(u32 index);
    T pop();

    // returns element at index
    T& at(u32 index);
    const T& at(u32 index) const;
    u32 size() const;
    // sets size to 0 - does not zero out internal memory or do any deallocation
    void clear();
    const T& operator[](u32 i) const
    {
        return at(i);
    }
    T& operator[](u32 i)
    {
        return at(i);
    }

    inline T* get_elements() { return elements; }

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

#include "me_hybrid_array.inl"
