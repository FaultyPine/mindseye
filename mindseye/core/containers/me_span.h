#pragma once

#include "mindseye/core/me_defines.h"

struct meSpan
{
    char* data;
    u64 size;
    
    meSpan() : data(0), size(0) {}
    template<typename T>
    meSpan(T* data, u64 size) : data((char*)data), size(size) {}
	template<typename T, unsigned int N>
	meSpan(T (&arr)[N]) : data((char*)arr), size(N) {}
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
	operator bool() const
	{
		return data && size;
	}

	bool isValid() const { return data != nullptr && size > 0; }
};

// for extra markup
typedef meSpan meOwningSpan;
typedef meSpan meNoOwnSpan;

template <typename T>
struct meSpanTyped : public meSpan
{
    T& operator[](size_t idx) 
	{
		ME_ASSERT(idx < size);
		return ((T*)data)[idx]; 
	}
	const T& operator[](size_t idx) const
	{
		ME_ASSERT(idx < size);
		return ((T*)data)[idx]; 
	}
};
