#pragma once

#include "core/me_defines.h"
#include "core/me_resourcepool.h"
#include "core/containers/dynarray.h"
#include "asset/me_asset.h"

typedef Eye meShaderID;

enum meUniformDataType
{
	UNKUNIFORMDATATYPE = 0,
    UNIFORM_FLOAT,
    UNIFORM_UINT,
    UNIFORM_SINT,
    UNIFORM_VEC2,
    UNIFORM_VEC3,
    UNIFORM_VEC4,
    UNIFORM_MAT3,
    UNIFORM_MAT4,
	UNIFORM_SAMPLER,
    NUM_UNIFORM_DATA_TYPES,
};

using meShaderFlags = u32;
enum meShaderFlags_
{
	// if set, reupload to gpu every frame
	meShaderFlags_AlwaysReupload,
	// if set, check IsDirty flag for if we should reupload
	meShaderFlags_UseDirty,
	meShaderFlags_IsDirty,

	meShaderFlags_Count,
};

struct meShaderUniform;
typedef void(*meShaderUniformUpdateCb)(meShaderUniform* uniform, const meMaterial& material, void* userData);

struct meShaderUniform
{
	// pointer into dedicated uniform cpu accessible memory block
	void* uniformData = nullptr; 
	// gpu handle
	u64 handle = 0;
	// call to repopulate uniformData with the most up-to-date data we should send to gpu
	meShaderUniformUpdateCb updateCb = nullptr;
	void* userData = nullptr;
	u32 flags = 0;

	// returns if we should reupload to gpu or not
	bool RefreshInternalUniformData(const meMaterial& material)
	{
		if (TEST_BIT(flags, meShaderFlags_AlwaysReupload)
			|| TEST_BIT(flags, meShaderFlags_IsDirty))
		{
			SET_BIT(flags, meShaderFlags_IsDirty, false);
			if (updateCb)
			{
				updateCb(this, material, userData);
			}
			return true;
		}
		return false;
	}
};

struct MEREFLECT(type) meShader : public meBaseAsset
{
    using meBaseAsset::meBaseAsset;
	MEREFLECT(exclude)
	DynArray<meShaderUniform> uniformHandles;
	MEREFLECT(exclude)
	u64 program;
};

struct meShaderPool : public meResourcePool<meShader>
{
	meShaderPool(
		meAllocator* resourceAllocator,
		meAllocator* payloadAllocator) :
	meResourcePool<meShader>(resourceAllocator, payloadAllocator) {}

	meAssetType GetAssetType() const
	{
		return MAShader;
	}

    // TODO: this sort of cache could probably be moved to the meResourcePool itself
    meMap<u64, meShaderID> shaderCache = {};
};

void meShaderInitialize(EngineContext* ctx);

meShaderID meShaderCreate(meSpan shaderBytecodeFS, meSpan shaderBytecodeVS);

meShaderPool& meShaderGetPool();

#ifndef ME_REFLECTING
#include "generatedtypes/me_shader.generated.h"
#endif
