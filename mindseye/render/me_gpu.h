#pragma once



struct meGPUBuffer
{
	u32 bufferHandle = U32_INVALID_ID;
	meSpan cpuData = {};

	bool IsValid() const { return bufferHandle != U32_INVALID_ID && cpuData; }
};