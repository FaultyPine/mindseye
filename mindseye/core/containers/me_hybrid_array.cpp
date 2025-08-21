#include "me_hybrid_array.h"

#include "core/me_log.h"
#include "core/me_memory.h"

constexpr u32 FIXEDGROWABLE_GROWTH_FACTOR = 2;
template <typename T, u32 fixedSize>
static bool CheckArrayResize(HybridArray<T, fixedSize>& array)
{
    ME_ASSERT(array.internalSize <= array.capacity);
    meAllocator* allocator = array.allocator;
    if (array.internalSize >= array.capacity)
    {
        if (!allocator)
        {
            // if we tried to resize without an allocator, that's no good
            LOG_ERROR("Tried to resize HybridArray with no allocator assigned!");
            return false;
        }
        array.capacity = array.internalSize * FIXEDGROWABLE_GROWTH_FACTOR;
        // moving old buffer (could be our fixed mem or a dyn alloc) to a new allocation
        //T* prevAlloc = array.elements == &array.fixedMem[0] ? nullptr : array.elements;
        T* prevElements = array.elements;
        array.elements = (T*)allocator->meAlloc(sizeof(T) * array.capacity);
        ME_MEMMOVE(array.elements, prevElements, sizeof(T) * array.internalSize);
        if (prevElements != &array.fixedMem[0])
        {
            allocator->meFree(prevElements);
        }
        // move elements from prev buffer to new one
        // notably... this will invalidate pointers to these elements. use indices or blocklist if that matters
    }
    return true;
}

template <typename T, u32 fixedSize>
HybridArray<T, fixedSize>::HybridArray(meAllocator* inAllocator)
{
    elements = &fixedMem[0];
    capacity = fixedSize;
    internalSize = 0;
    allocator = inAllocator ? inAllocator : GetSystemAllocator();
}

template <typename T, u32 fixedSize>
HybridArray<T, fixedSize>::~HybridArray()
{
    if (elements != &fixedMem[0])
    {
        allocator->meFree(elements);
    }
}

template <typename T, u32 fixedSize>
void HybridArray<T, fixedSize>::push_back(const T& element)
{
    if (!CheckArrayResize(*this))
    {
        return;
    }
    u32 idx = internalSize;
    internalSize++;
    at(idx) = element;
}

template <typename T, u32 fixedSize>
void HybridArray<T, fixedSize>::insert(const T& element, u32 index)
{
    if (!CheckArrayResize(*this))
    {
        return;
    }
    if (index >= internalSize)
    {
        LOG_ERROR("Attempted to insert into HybridArray at invalid index");
        return;
    }
    u64 prevSize = internalSize++;
    if (index == prevSize)
    {
        at(prevSize) = element;
    }
    else
    {
        u32 elementsToMove = prevSize - index;
        ME_MEMMOVE(&at(index + 1), &at(index), sizeof(T) * elementsToMove);
        at(index) = element;
    }
}

template <typename T, u32 fixedSize>
T HybridArray<T, fixedSize>::erase_and_fill(u32 index)
{
    if (index >= internalSize)
    {
        LOG_ERROR("attempted to erase invalid index");
        return {};
    }
    T tmp = at(index); // copy
    // swap element to erase with last element
    at(index) = at(internalSize-1);
    internalSize--;
    return tmp;
}

template <typename T, u32 fixedSize>
T HybridArray<T, fixedSize>::erase(u32 index)
{
    if (index >= internalSize)
    {
        LOG_ERROR("attempted to erase invalid index");
        return {};
    }
    T tmp = at(index); // copy
    // move everything from the right of this index to the left
    size_t moveSize = sizeof(T) * (internalSize-index);
    ME_MEMMOVE(&at(index), &at(index+1), moveSize); 
    internalSize--;
    return tmp;
}

template <typename T, u32 fixedSize>
T HybridArray<T, fixedSize>::pop()
{
    ME_ASSERT(internalSize > 0 && "Cannot pop from empty array");
    T value = at(internalSize - 1);
    --internalSize;
    return value;
}

template <typename T, u32 fixedSize>
T& HybridArray<T, fixedSize>::at(u32 index)
{
    ME_ASSERT(index < internalSize);
    return elements[index];
}
template <typename T, u32 fixedSize>
const T& HybridArray<T, fixedSize>::at(u32 index) const
{
    ME_ASSERT(index < internalSize);
    return elements[index];
}

template <typename T, u32 fixedSize>
u32 HybridArray<T, fixedSize>::size() const
{
    return internalSize;
}

template <typename T, u32 fixedSize>
void HybridArray<T, fixedSize>::clear()
{
    internalSize = 0;
}


void HybridArrayTests()
{
    LOG_INFO("Running HybridArray tests...");
    constexpr u32 testFixedSize = 10;
    HybridArray<u32, testFixedSize> arr(GetSystemAllocator());
    ME_ASSERT(arr.internalSize == 0);
    arr.push_back(0);
    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);
    ME_ASSERT(arr.internalSize == 4);
    ME_ASSERT(arr.elements == &arr.fixedMem[0]);
    while (arr.internalSize < testFixedSize)
    {
        arr.push_back(arr.internalSize);
    }
    for(u32 i = 0; i < arr.internalSize; i++)
    {
        LOG_INFO("arr[%i] = %i", i, arr.at(i));
    }
    LOG_INFO("Filled array");
    ME_ASSERT(arr.elements == &arr.fixedMem[0]);
    arr.push_back(testFixedSize); // this should trigger a reallocation of the entire memory
    ME_ASSERT(arr.elements != &arr.fixedMem[0]);
    ME_ASSERT(arr.erase(0) == 0);
    LOG_INFO("Removed first element. New first element: %i", arr.at(0));
    ME_ASSERT(arr.at(0) == 1);
    ME_ASSERT(arr.internalSize == testFixedSize);
    ME_ASSERT(arr.elements != &arr.fixedMem[0]); // despite dropping back below fixedSize, we should still be using the dynamic mem
    ME_ASSERT(arr.at(arr.internalSize-1) == testFixedSize);
    ME_ASSERT(arr.erase_and_fill(0) == 1); // removes "1" and puts the last element "testFixedSize" in its place
    ME_ASSERT(arr.at(0) == testFixedSize); // first element should now be what was the last element
    arr.insert(99, 1);
    ME_ASSERT(arr.at(0) == testFixedSize); // should still be the case...
    ME_ASSERT(arr.at(1) == 99);
    ME_ASSERT(arr.at(2) == 2);
    ME_ASSERT(arr.at(arr.internalSize-1) == testFixedSize);
    arr.clear();
    ME_ASSERT(arr.internalSize == 0);
    //arr.insert(999, 3); // should report an error
    LOG_INFO("HybridArray tests successful!");
}