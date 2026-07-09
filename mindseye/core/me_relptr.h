#pragma once

#include "core/me_defines.h"

struct MEREFLECT(type) 
meRelPtr
{
    using OffsetType = s64;
    OffsetType offset;

    template <typename T>
    T* Get()
    {
        return offset ? (T*)((char*)&offset + offset) : nullptr;
    }
    template <typename T>
    const T* Get() const
    {
        return offset ? (const T*)((const char*)&offset + offset) : nullptr;
    }
    template <typename T>
    T* operator->()
    {
        return Get<T>();
    }
    template <typename T>
    const T* operator->() const
    {
        return Get<T>();
    }
    template <typename T>
    T& operator*()
    {
        return *Get<T>();
    }
    template <typename T>
    const T& operator*() const
    {
        return *Get<T>();
    }
    template <typename T>
    void operator=(T* ptr)
    {
        offset = ptr ? (OffsetType)((char*)ptr - (char*)&offset) : 0;
    }
    void Clear()
    {
        offset = 0;
    }
    explicit operator bool() const
    {
        return offset != 0;
    }
};


// an Offset Pointer is really just an offset (from some arbitrary base pointer)
template <typename T>
struct meOffsetPtr
{
    u32 offset;

};
