#pragma once

#include "core/me_defines.h"
#include "core/containers/dynarray.h"

struct meShaderUniform;
typedef MAID meShaderID;
struct meShader
{
	DynArray(meShaderUniform) uniformHandles;
	u64 program;
};

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

enum meShaderFlags : u32;
enum meShaderFlags_
{
	// if set, reupload to gpu every frame
	meShaderFlags_AlwaysReupload,
	// if set, check IsDirty flag for if we should reupload
	meShaderFlags_UseDirty,
	meShaderFlags_IsDirty,

	meShaderFlags_Count,
};

typedef void(*meShaderUniformUpdateCb)(meShaderUniform* uniform, void* userData);

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
	bool RefreshInternalUniformData()
	{
		if (TEST_BIT(flags, meShaderFlags_AlwaysReupload)
			|| TEST_BIT(flags, meShaderFlags_IsDirty))
		{
			SET_BIT(flags, meShaderFlags_IsDirty, false);
			if (updateCb)
			{
				updateCb(this, userData);
			}
			return true;
		}
		return false;
	}
};

struct meShaderPool : public meResourcePool<meShader, meShaderPool>
{
	meShaderPool(
		meAllocator* resourceAllocator,
		meAllocator* payloadAllocator) :
	meResourcePool<meShader, meShaderPool>(resourceAllocator, payloadAllocator) {}

	meAssetType GetAssetType() const
	{
		return MAShader;
	}
};

void meShaderInitialize(EngineContext* ctx);

meShaderPool& meShaderGetPool();
