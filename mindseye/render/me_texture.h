#pragma once

#include "core/me_resourcepool.h"
#include "me_gpu.h"
struct cgltf_image;

typedef Eye meTextureID;
struct meTexture
{
	static constexpr u32 METEXTURE_MAX_NAME_LEN = 50;
	char name[METEXTURE_MAX_NAME_LEN];
	meGPUBuffer buffer = {};
	u64 sampler = U64_INVALID_ID;
	// will likely also put tex format, width/height, etc
	bool IsValid() const { return buffer.IsValid() && sampler != U64_INVALID_ID; }
};

typedef u32 meTextureFormat;
enum meTextureFormat_
{
	meTextureFormat_UNK = 0,
	meTextureFormat_RGBA8,
};

struct meTextureLoadParams
{
	u32 width = 0;
	u32 height = 0;
	bool hasMips = false;
	u32 numLayers = 1;
	meTextureFormat textureFormat = meTextureFormat_UNK;
	u64 flags = 0;
	meSpan mem = {};
};

struct meTexturePool : public meResourcePool<meTexture>
{
	using meResourcePool<meTexture>::Load;

	meTexturePool(
		meAllocator* resourceAllocator,
		meAllocator* payloadAllocator) :
	meResourcePool<meTexture>(resourceAllocator, payloadAllocator) {}

	meAssetType GetAssetType() const
	{
		return MATexture;
	}

	meGPUBuffer Load(
		RendererFrontend* renderer,
		StringView gltfResPath,
		const cgltf_image& gltfImage);

	meGPUBuffer Load(const meTextureLoadParams& params);
};

void meTextureInitialize(EngineContext* ctx);

meTexturePool& meTextureGetPool();