#pragma once

#include "core/me_handle.h"
#include "me_gpu.h"

struct meTexture
{
	static constexpr u32 METEXTURE_MAX_NAME_LEN = 50;
	char name[METEXTURE_MAX_NAME_LEN];
	meGPUBuffer buffer;
	u64 sampler;
	// will likely also put tex format, width/height, etc
};

struct meTexturePool : public meResourcePool<meTexture>
{
	meTexturePool(
		meAllocator* resourceAllocator,
		meAllocator* payloadAllocator) : 
	meResourcePool<meTexture>(resourceAllocator, payloadAllocator) {}

};

void meTextureInitialize(EngineContext* ctx);

meTexturePool& meTextureGetPool();