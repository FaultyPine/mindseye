#include "me_blocklist.h"



template <typename T, u32 BLOCK_SIZE>
meBlockList<T, BLOCK_SIZE>::meBlockList(meAllocator* alloc)
    : allocator(alloc) {}

template <typename T, u32 BLOCK_SIZE>
meBlockList<T, BLOCK_SIZE>::~meBlockList()
{
    clear();
}

template <typename T, u32 BLOCK_SIZE>
u32 meBlockList<T, BLOCK_SIZE>::push(const T& value)
{
    Block* blk = head;
    u32 blockIdx = 0;
    while (blk && blk->count == BLOCK_SIZE)
    {
        blk = blk->next;
        ++blockIdx;
    }

    if (!blk) 
    {
        blk = MENEW(allocator, Block);
        if (tail) tail->next = blk;
        else head = blk;
        tail = blk;
    }

    // Find a free slot if available
    u32 idx = 0;
    bool foundFree = false;
    for (; idx < BLOCK_SIZE; ++idx)
    {
        if (blk->freeBits.get(idx))
        {
            foundFree = true;
            break;
        }
    }

    if (foundFree)
    {
        blk->data[idx] = value;
        blk->freeBits.set(idx, false);
        ++blk->count;
        ++size;
        return blockIdx * BLOCK_SIZE + idx;
    }
    else 
	{ 
		ME_ASSERT(false && "something has gone terribly wrong"); 
	}
    return 0;
}

template <typename T, u32 BLOCK_SIZE>
void meBlockList<T, BLOCK_SIZE>::markDeleted(u32 index)
{
    Block* blk = head;
    u32 blockIdx = index / BLOCK_SIZE;
    u32 slotIdx = index % BLOCK_SIZE;
    for (u32 i = 0; i < blockIdx && blk; ++i)
    {
        blk = blk->next;
    }
    ME_ASSERT(blk && "Index out of bounds in markDeleted");
    ME_ASSERT(!blk->freeBits.get(slotIdx) && "Deleting a free slot in meBlockList");

    blk->freeBits.set(slotIdx, true);
    --blk->count;
    --size;
}

template <typename T, u32 BLOCK_SIZE>
void meBlockList<T, BLOCK_SIZE>::clear()
{
    Block* blk = head;
    while (blk) 
    {
        // Reset freelist for each block before deletion
        blk->freeBits.clear();
        blk->count = 0;

        Block* next = blk->next;
        MEDELETE(allocator, Block, blk);
        blk = next;
    }
    head = tail = nullptr;
    size = 0;
}

template <typename T, u32 BLOCK_SIZE>
const T& meBlockList<T, BLOCK_SIZE>::get(u32 index) const
{
    Block* blk = head;
    u32 blockIdx = index / BLOCK_SIZE;
    u32 slotIdx = index % BLOCK_SIZE;
    for (u32 i = 0; i < blockIdx && blk; ++i)
    {
        blk = blk->next;
    }
    ME_ASSERT(blk && "Index out of bounds in meBlockList::get");
    ME_ASSERT(!blk->freeBits.get(slotIdx) && "Getting a free slot in meBlockList");

    return blk->data[slotIdx];
}


template <typename T, u32 BLOCK_SIZE>
meBlockList<T, BLOCK_SIZE>::Iterator::Iterator(Block* b, u32 i)
    : blk(b), idx(i) {}

template <typename T, u32 BLOCK_SIZE>
void meBlockList<T, BLOCK_SIZE>::Iterator::skip_free()
{
    while (blk)
    {
        while (idx < BLOCK_SIZE && blk->freeBits.get(idx))
            ++idx;
        if (idx < BLOCK_SIZE && (!blk->freeBits.get(idx)))
            break;
        blk = blk->next;
        idx = 0;
    }
}

template <typename T, u32 BLOCK_SIZE>
T& meBlockList<T, BLOCK_SIZE>::Iterator::operator*()
{
    return blk->data[idx];
}

template <typename T, u32 BLOCK_SIZE>
typename meBlockList<T, BLOCK_SIZE>::Iterator& meBlockList<T, BLOCK_SIZE>::Iterator::operator++()
{
    ++idx;
    skip_free();
    return *this;
}

template <typename T, u32 BLOCK_SIZE>
bool meBlockList<T, BLOCK_SIZE>::Iterator::operator==(const Iterator& other) const
{
    return blk == other.blk && idx == other.idx; // this compares pointers - intentionally
}

template <typename T, u32 BLOCK_SIZE>
bool meBlockList<T, BLOCK_SIZE>::Iterator::operator!=(const Iterator& other) const
{
    return blk != other.blk || idx != other.idx;
}

template <typename T, u32 BLOCK_SIZE>
typename meBlockList<T, BLOCK_SIZE>::Iterator meBlockList<T, BLOCK_SIZE>::begin()
{
    Iterator it(head, 0);
    it.skip_free();
    return it;
}

template <typename T, u32 BLOCK_SIZE>
typename meBlockList<T, BLOCK_SIZE>::Iterator meBlockList<T, BLOCK_SIZE>::end()
{
    return Iterator(nullptr, 0);
}


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
