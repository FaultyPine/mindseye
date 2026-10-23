#include "me_hybrid_array.h"

void HybridArrayTests()
{
    LOG_INFO("Running HybridArray tests...");
    constexpr u32 testFixedSize = 10;
    HybridArray<u32, testFixedSize> arr(GetDefaultAllocator());
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