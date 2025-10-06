#pragma once


#include "core/me_handle.h"

enum meMaterialTextureType : u32 
{
    DIFFUSE = 0,
    NORMALS,
    ROUGHNESS,
    METALLIC,
	AO,
	EMISSIVE,
	DISPLACEMENT,
    OPACITY,

    NUM_MATERIAL_TEXTURE_TYPES,
};

struct meMaterial
{
	Eye shaderHandle;
	Eye textureHandles[NUM_MATERIAL_TEXTURE_TYPES];
};

struct meMaterialPool : public meResourcePool<meMaterial>
{
	meMaterialPool(
		meAllocator* resourceAllocator,
		meAllocator* payloadAllocator) :
	meResourcePool<meMaterial>(resourceAllocator, payloadAllocator) {}
};


void meMaterialInitialize(EngineContext* engine);

meMaterialPool& meMaterialGetPool();

