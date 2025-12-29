#pragma once

#include "core/me_resourcepool.h"
struct cgltf_material;


typedef Eye meMaterialID;

#define ME_MATERIAL_TEXTURE_TYPE_NAMES \
X(Diffuse) \
X(Normals) \
X(Roughness) \
X(Metallic) \
X(AO) \
X(Emissive) \
X(Displacement) \
X(Opacity) \
X(NUM_MATERIAL_TEXTURE_TYPES)

enum meMaterialTextureType : u32 
{
    #define X(matname) matname,
	ME_MATERIAL_TEXTURE_TYPE_NAMES
	#undef X
};

struct meMaterial
{
	static constexpr u32 MEMATERIAL_MAX_NAME_LEN = 50;
	char name[MEMATERIAL_MAX_NAME_LEN];
	Eye shaderHandle = {};
	Eye textureHandles[NUM_MATERIAL_TEXTURE_TYPES];

	meMaterial()
	{
		ME_MEMCLEAR(name, MEMATERIAL_MAX_NAME_LEN);
		for (u32 i = 0; i < NUM_MATERIAL_TEXTURE_TYPES; i++)
		{
			textureHandles[i] = {};
		}
		shaderHandle = {};
	}
};

struct meMaterialPool : public meResourcePool<meMaterial>
{
	meMaterialPool(
		meAllocator* resourceAllocator,
		meAllocator* payloadAllocator) :
	meResourcePool<meMaterial>(resourceAllocator, payloadAllocator) {}

	meMaterialID Load(
		RendererFrontend* renderer,
		StringView gltfResPath,
		const cgltf_material& gltfMaterial);
};


void meMaterialInitialize(EngineContext* engine);

meMaterialPool& meMaterialGetPool();

StringView meMaterialGetTextureTypeName(meMaterialTextureType texType);
