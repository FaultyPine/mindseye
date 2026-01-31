#pragma once

#include "core/me_defines.h"

template <typename T, u32 N>
struct meArray
{
	const T& operator[](u32 idx) const
	{
		ME_ASSERT(idx < N);
		return data[idx];
	}
	
	T& operator[](u32 idx)
	{
		ME_ASSERT(idx < N);
		return data[idx];
	}

	u32 size;
	T data[N];
};
