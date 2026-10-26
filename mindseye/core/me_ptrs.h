#pragma once

#include "core/me_defines.h"

struct MEREFLECT(type) 
meRelPtr
{
    using OffsetType = s64;
    OffsetType offset = 0;

    meRelPtr() = default;
    template <typename T>
    meRelPtr(T* ptr)
    {
        offset = ptr ? (OffsetType)((char*)ptr - (char*)&offset) : 0;
    }

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

void meRelPtrTests();

#include <memory>

template <typename T>
using meUniquePtr = std::unique_ptr<T>;

template <typename T>
using meSharedPtr = std::shared_ptr<T>;