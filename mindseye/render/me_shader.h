#pragma once


#include "core/containers/dynarray.h"

struct meShaderUniform;

struct meShader
{
	DynArray(meShaderUniform) uniformHandles;

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
    NUM_UNIFORM_DATA_TYPES,
};

enum meUniformUsageFrequency
{
	TYPICAL = 0,
	INFREQUENT,
	FREQUENT,
};

struct meShaderUniform
{
	// pointer into dedicated uniform cpu accessible memory block
	void* uniformData = nullptr; 
	// gpu handle
	u64 handle = 0;
	meUniformDataType dataType = UNKUNIFORMDATATYPE;
	meUniformUsageFrequency usageFreq = TYPICAL;
	bool dirty = false;
};

