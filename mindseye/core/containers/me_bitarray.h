#pragma once

#include "core/me_defines.h"

template <u32 N>
struct meBitArray
{
    using Word = u32;
    static constexpr u32 BITS_PER_WORD = 32;
    static constexpr u32 NUM_WORDS = (N + BITS_PER_WORD - 1) / BITS_PER_WORD;

    Word data[NUM_WORDS] = {};

    meBitArray(bool isInitiallySet = false) 
    {
        ME_MEMSET(&data, isInitiallySet ? ~0 : 0, sizeof(data));
    }

    bool get(u32 idx) const
    {
        u8 nthBit = idx % BITS_PER_WORD;
        const Word& w = data[idx / BITS_PER_WORD];
        return TEST_BIT(w, nthBit);
    }

    void set(u32 idx, bool value)
    {
        ME_ASSERT(idx < N);
        u8 nthBit = idx % BITS_PER_WORD;
        Word& w = data[idx / BITS_PER_WORD];
        w = SET_BIT(w, nthBit, value);
    }

    void clear()
    {
        ME_MEMCLEAR(&data, sizeof(data));
    }

    u32 count() const { return N; }
};



void TestmeBitArray() 
{
    meBitArray<64> bits;
    ME_ASSERT(bits.count() == 64);

    // All bits should be false by default
    for (u32 i = 0; i < 64; ++i)
        ME_ASSERT(bits.get(i) == false);

    // Set every even bit to true
    for (u32 i = 0; i < 64; i += 2)
        bits.set(i, true);

    // Check even bits are true, odd bits are false
    for (u32 i = 0; i < 64; ++i)
    {
        if (i % 2 == 0)
        {
            ME_ASSERT(bits.get(i) == true);
        }
        else
        {
            ME_ASSERT(bits.get(i) == false);
        }
    }

    // Flip all bits
    for (u32 i = 0; i < 64; ++i)
        bits.set(i, !bits.get(i));

    // Now odd bits should be true, even bits false
    for (u32 i = 0; i < 64; ++i)
    {
        if (i % 2 == 1)
        {
            ME_ASSERT(bits.get(i) == true);

        }
        else
        {
            ME_ASSERT(bits.get(i) == false);
        }
    }

    // Set and unset a single bit
    bits.set(10, true);
    ME_ASSERT(bits.get(10) == true);
    bits.set(10, false);
}