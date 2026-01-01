#pragma once

#include "core/me_defines.h"
#include "core/containers/me_bitarray.h"
#include "core/me_memory.h"

// A simple blocklist (colony/hive) implementation.
// Stores elements in blocks for stable references and efficient insertion/removal.
// This is a minimal version inspired by P0447R21.

template <typename T, u32 BLOCK_SIZE = 32>
struct meBlockList
{
    struct Block 
    {
        T data[BLOCK_SIZE];
        u32 count = 0;
        Block* next = nullptr;
        meBitArray<BLOCK_SIZE> freeBits = meBitArray<BLOCK_SIZE>(true); // true = slot is free, false = used
    };

    Block* head = nullptr;
    Block* tail = nullptr;
    u32 size = 0;
    meAllocator* allocator = nullptr;

    meBlockList(meAllocator* alloc = nullptr);
    ~meBlockList();

    u32 push(const T& value);
    void markDeleted(u32 index);
    void clear();
    const T& get(u32 index) const;
	T& get(u32 index) { return const_cast<T&>(const_cast<const meBlockList*>(this)->get(index)); }
	bool empty() const { return size == 0; }

    struct Iterator 
    {
        Block* blk;
        u32 idx;

        Iterator(Block* b, u32 i);

        T& operator*();
        Iterator& operator++();
        bool operator==(const Iterator& other) const;
        bool operator!=(const Iterator& other) const;
        void skip_free();
    };

    Iterator begin();
    Iterator end();
};

MEAPI void TestBlocklist();