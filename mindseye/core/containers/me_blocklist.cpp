#include "me_blocklist.h"

void TestBlocklist() 
{
    meAllocator* alloc = GetDefaultAllocator();
    meBlockList<int, 5> list(alloc);

    // Test push and size
    for (int i = 0; i < 10; ++i)
        ME_ASSERT(list.push(i) == (u32)i);
    ME_ASSERT(list.size == 10);

    // Test get
    for (int i = 0; i < 10; ++i)
        ME_ASSERT(list.get(i) == i);

    // Test iteration
    int expected = 0;
    for (auto it = list.begin(); it != list.end(); ++it)
    {
        int t = *it;
        ME_ASSERT(t == expected);
        ++expected;
    }
    ME_ASSERT(expected == 10);

    // Test markDeleted (remove index 5)
    list.markDeleted(5);
    ME_ASSERT(list.size == 9);

    // Check that 5 is gone and others are correct
    bool found5 = false;
    int count = 0;
    for (auto it = list.begin(); it != list.end(); ++it)
    {
        if (*it == 5) found5 = true;
        ++count;
    }
    ME_ASSERT(!found5);
    ME_ASSERT(count == 9);

    // Test push after deletion reuses the stable physical index
    ME_ASSERT(list.push(10) == 5);
    ME_ASSERT(list.get(5) == 10);

    // Test begin skips a deleted first slot
    list.markDeleted(0);
    ME_ASSERT(*list.begin() == 1);

    // Test clear
    list.clear();
    ME_ASSERT(list.size == 0);
    ME_ASSERT(list.begin() == list.end());
}
