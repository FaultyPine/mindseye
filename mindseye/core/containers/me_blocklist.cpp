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
void meBlockList<T, BLOCK_SIZE>::push(const T& value)
{
    if (!tail || tail->count == BLOCK_SIZE) 
    {
        Block* newBlock = MENEW(allocator, Block);
        if (tail) tail->next = newBlock;
        else head = newBlock;
        tail = newBlock;
    }

    // Find a free slot if available
    u32 idx = 0;
    bool foundFree = false;
    for (; idx < BLOCK_SIZE; ++idx)
    {
        if (tail->freeBits.get(idx))
        {
            foundFree = true;
            break;
        }
    }

    if (foundFree)
    {
        tail->data[idx] = value;
        tail->freeBits.set(idx, false);
        ++tail->count;
        ++size;
    }
    else { ME_ASSERT(false && "something has gone terribly wrong"); }
}

template <typename T, u32 BLOCK_SIZE>
void meBlockList<T, BLOCK_SIZE>::markDeleted(u32 index)
{
    ME_ASSERT(index < size);

    Block* blk = head;
    u32 remaining = index;

    while (blk)
    {
        u32 usedInBlock = 0;
        for (u32 i = 0; i < BLOCK_SIZE; ++i)
        {
            if (!blk->freeBits.get(i))
                ++usedInBlock;
        }

        if (remaining < usedInBlock)
        {
            // Find the N-th used slot in this block
            for (u32 i = 0; i < BLOCK_SIZE; ++i)
            {
                if (!blk->freeBits.get(i))
                {
                    if (remaining == 0)
                    {
                        blk->freeBits.set(i, true);
                        --size;
                        return;
                    }
                    --remaining;
                }
            }
        }
        else
        {
            remaining -= usedInBlock;
            blk = blk->next;
        }
    }
    ME_ASSERT(false && "Index out of bounds in markDeleted");
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
T& meBlockList<T, BLOCK_SIZE>::get(u32 index)
{
    ME_ASSERT(index < size);

    Block* blk = head;
    u32 remaining = index;

    while (blk)
    {
        u32 usedInBlock = 0;
        // Count only used (non-free) slots in this block
        for (u32 i = 0; i < BLOCK_SIZE; ++i)
        {
            if (!blk->freeBits.get(i))
                ++usedInBlock;
        }

        if (remaining < usedInBlock)
        {
            // Find the N-th used slot in this block
            for (u32 i = 0; i < BLOCK_SIZE; ++i)
            {
                if (!blk->freeBits.get(i))
                {
                    if (remaining == 0)
                        return blk->data[i];
                    --remaining;
                }
            }
        }
        else
        {
            remaining -= usedInBlock;
            blk = blk->next;
        }
    }

    // Should never reach here if index < size
    ME_ASSERT(false && "Index out of bounds in meBlockList::get");
    return head->data[0]; // fallback
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
    return Iterator(head, 0);
}

template <typename T, u32 BLOCK_SIZE>
typename meBlockList<T, BLOCK_SIZE>::Iterator meBlockList<T, BLOCK_SIZE>::end()
{
    return Iterator(nullptr, 0);
}


void TestBlocklist() 
{
    meAllocator* alloc = GetSystemAllocator();
    meBlockList<int, 5> list(alloc);

    // Test push and size
    for (int i = 0; i < 10; ++i)
        list.push(i);
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

    // Test get after deletion (should skip deleted)
    for (u32 i = 0; i < list.size; ++i)
    {
        int val = list.get(i);
        ME_ASSERT(val != 5);
    }

    // Test clear
    list.clear();
    ME_ASSERT(list.size == 0);
    ME_ASSERT(list.begin() == list.end());
}