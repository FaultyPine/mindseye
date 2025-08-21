#pragma once

#include "mindseye/core/me_defines.h"

struct meSpan
{
    void* data;
    u64 size;
    
    meSpan() : data(0), size(0) {}
    template<typename T>
    meSpan(T* data, u64 size) : data((void*)data), size(size) {}
    meSpan Subspan(u64 offset)
    {
        ME_ASSERT(size >= offset);
        return meSpan(((u8*)data)+offset, size - offset);
    }
    template<typename T>
    operator T*() const 
    {
        return (T*)data; 
    }

	bool isValid() const { return data != nullptr && size > 0; }
};

// for extra markup
typedef meSpan meOwningSpan;
typedef meSpan meNoOwnSpan;