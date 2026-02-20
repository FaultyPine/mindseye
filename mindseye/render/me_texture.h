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
	u32 samplingFlags = ME_UINT_MAX;
	// will likely also put tex format, width/height, etc
	bool IsValid() const { return buffer.IsValid() && sampler != U64_INVALID_ID; }
};

typedef u32 meTextureSamplingFlags;

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






// NOTE: currently, these just map to the same values as BGFX. Would be good to not do that, to ensure the renderer "backend" is separate from the frontend
#define ME_UINT32_C(x) x##ui32
enum meTextureSamplingFlags_
{
	ME_SAMPLER_U_MIRROR =                     ME_UINT32_C(0x00000001), //!< Wrap U mode: Mirror
	ME_SAMPLER_U_CLAMP =                      ME_UINT32_C(0x00000002), //!< Wrap U mode: Clamp
	ME_SAMPLER_U_BORDER =                     ME_UINT32_C(0x00000003), //!< Wrap U mode: Border
	ME_SAMPLER_U_SHIFT =                      0,

	ME_SAMPLER_U_MASK =                       ME_UINT32_C(0x00000003),

	ME_SAMPLER_V_MIRROR =                     ME_UINT32_C(0x00000004), //!< Wrap V mode: Mirror
	ME_SAMPLER_V_CLAMP =                      ME_UINT32_C(0x00000008), //!< Wrap V mode: Clamp
	ME_SAMPLER_V_BORDER =                     ME_UINT32_C(0x0000000c), //!< Wrap V mode: Border
	ME_SAMPLER_V_SHIFT =                      2,

	ME_SAMPLER_V_MASK =                       ME_UINT32_C(0x0000000c),

	ME_SAMPLER_W_MIRROR =                     ME_UINT32_C(0x00000010), //!< Wrap W mode: Mirror
	ME_SAMPLER_W_CLAMP =                      ME_UINT32_C(0x00000020), //!< Wrap W mode: Clamp
	ME_SAMPLER_W_BORDER =                     ME_UINT32_C(0x00000030), //!< Wrap W mode: Border
	ME_SAMPLER_W_SHIFT =                      4,

	ME_SAMPLER_W_MASK =                       ME_UINT32_C(0x00000030),

	ME_SAMPLER_MIN_POINT =                    ME_UINT32_C(0x00000040), //!< Min sampling mode: Point
	ME_SAMPLER_MIN_ANISOTROPIC =              ME_UINT32_C(0x00000080), //!< Min sampling mode: Anisotropic
	ME_SAMPLER_MIN_SHIFT =                    6,

	ME_SAMPLER_MIN_MASK =                     ME_UINT32_C(0x000000c0),

	ME_SAMPLER_MAG_POINT =                    ME_UINT32_C(0x00000100), //!< Mag sampling mode: Point
	ME_SAMPLER_MAG_ANISOTROPIC =              ME_UINT32_C(0x00000200), //!< Mag sampling mode: Anisotropic
	ME_SAMPLER_MAG_SHIFT =                    8,

	ME_SAMPLER_MAG_MASK =                     ME_UINT32_C(0x00000300),

	ME_SAMPLER_MIP_POINT =                    ME_UINT32_C(0x00000400), //!< Mip sampling mode: Point
	ME_SAMPLER_MIP_SHIFT =                    10,

	ME_SAMPLER_MIP_MASK =                     ME_UINT32_C(0x00000400),

	ME_SAMPLER_COMPARE_LESS =                 ME_UINT32_C(0x00010000), //!< Compare when sampling depth texture: less.
	ME_SAMPLER_COMPARE_LEQUAL =               ME_UINT32_C(0x00020000), //!< Compare when sampling depth texture: less or equal.
	ME_SAMPLER_COMPARE_EQUAL =                ME_UINT32_C(0x00030000), //!< Compare when sampling depth texture: equal.
	ME_SAMPLER_COMPARE_GEQUAL =               ME_UINT32_C(0x00040000), //!< Compare when sampling depth texture: greater or equal.
	ME_SAMPLER_COMPARE_GREATER =              ME_UINT32_C(0x00050000), //!< Compare when sampling depth texture: greater.
	ME_SAMPLER_COMPARE_NOTEQUAL =             ME_UINT32_C(0x00060000), //!< Compare when sampling depth texture: not equal.
	ME_SAMPLER_COMPARE_NEVER =                ME_UINT32_C(0x00070000), //!< Compare when sampling depth texture: never.
	ME_SAMPLER_COMPARE_ALWAYS =               ME_UINT32_C(0x00080000), //!< Compare when sampling depth texture: always.
	ME_SAMPLER_COMPARE_SHIFT =                16,

	ME_SAMPLER_COMPARE_MASK =                 ME_UINT32_C(0x000f0000),

	ME_SAMPLER_BORDER_COLOR_SHIFT =           24,

	ME_SAMPLER_BORDER_COLOR_MASK =            ME_UINT32_C(0x0f000000),
	//ME_SAMPLER_BORDER_COLOR = ( ( (uint32_t)(v)<<ME_SAMPLER_BORDER_COLOR_SHIFT )&ME_SAMPLER_BORDER_COLOR_MASK),

	ME_SAMPLER_RESERVED_SHIFT =               28,

	ME_SAMPLER_RESERVED_MASK =                ME_UINT32_C(0xf0000000),

	ME_SAMPLER_NONE =                         ME_UINT32_C(0x00000000),
	ME_SAMPLER_SAMPLE_STENCIL =               ME_UINT32_C(0x00100000), //!< Sample stencil instead of depth.
	ME_SAMPLER_POINT = (0 
                        | ME_SAMPLER_MIN_POINT 
                        | ME_SAMPLER_MAG_POINT 
                        | ME_SAMPLER_MIP_POINT),

    ME_SAMPLER_UVW_MIRROR = (0 
                            | ME_SAMPLER_U_MIRROR 
                            | ME_SAMPLER_V_MIRROR 
                            | ME_SAMPLER_W_MIRROR),

    ME_SAMPLER_UVW_CLAMP = (0 
                            | ME_SAMPLER_U_CLAMP 
                            | ME_SAMPLER_V_CLAMP 
                            | ME_SAMPLER_W_CLAMP),

    ME_SAMPLER_UVW_BORDER = (0 
                            | ME_SAMPLER_U_BORDER 
                            | ME_SAMPLER_V_BORDER 
                            | ME_SAMPLER_W_BORDER),

    ME_SAMPLER_BITS_MASK = (0 
                            | ME_SAMPLER_U_MASK 
                            | ME_SAMPLER_V_MASK 
                            | ME_SAMPLER_W_MASK 
                            | ME_SAMPLER_MIN_MASK 
                            | ME_SAMPLER_MAG_MASK 
                            | ME_SAMPLER_MIP_MASK 
                            | ME_SAMPLER_COMPARE_MASK),
};