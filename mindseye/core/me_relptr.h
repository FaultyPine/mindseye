#pragma once

struct MEREFLECT(type) 
meRelPtr
{
    using OffsetType = s64;
    OffsetType offset;

    template <typename T>
    T* operator->()
    {
        return offset ? (T*)((char*)&offset + offset) : nullptr;
    }
    template <typename T>
    T& operator*()
    {
        return *this->operator-><T>();
    }
    template <typename T>
    void operator=(T* ptr)
    {
        offset = ptr ? (OffsetType)((char*)ptr - (char*)&offset) : 0;
    }
};


// an Offset Pointer is really just an offset (from some arbitrary base pointer)
template <typename T>
struct meOffsetPtr
{
    u32 offset;

};