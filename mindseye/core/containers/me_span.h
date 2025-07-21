#pragma once

#include "mindseye/core/me_defines.h"

struct meSpan
{
    void* data;
    u64 size;

    template<typename T>
    meSpan(T* data, u64 size) : data((void*)data), size(size) {}
    meSpan Subspan(u64 offset)
    {
        ME_ASSERT(size >= offset);
        return meSpan(((u8*)data)+offset, size - offset);
    }
};

// for extra markup
typedef meSpan meOwningSpan;
typedef meSpan meNoOwnSpan;