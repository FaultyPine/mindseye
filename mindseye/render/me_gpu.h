#pragma once

#include "core/me_core.h"

struct MEREFLECT(type) meGPUBuffer
{
    MEREFLECT(exclude)
	u32 bufferHandle = U32_INVALID_ID;
	meSpan cpuData = {};

	bool IsValid() const { return bufferHandle != U32_INVALID_ID && cpuData; }
};

#ifndef ME_REFLECTING
#include "generatedtypes/me_gpu.generated.h"
#endif
