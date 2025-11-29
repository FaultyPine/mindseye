#pragma once

#include "core/me_core.h"
#include "me_gpu.h"
#include "core/me_resourcepool.h"
#include "render/me_material.h"
#include "render/renderer_frontend.h"

struct cgltf_mesh;

typedef Eye meMeshID;

typedef u32 meMeshVertexLayoutType;
enum meMeshVertexLayoutType_
{
	meMeshVertexLayoutType_Index,
	meMeshVertexLayoutType_Position,
	meMeshVertexLayoutType_Normal,
	meMeshVertexLayoutType_Tangent,
	meMeshVertexLayoutType_TexCoord0,
	meMeshVertexLayoutType_Color,
	meMeshVertexLayoutType_Weights,
};

struct meMesh
{
	String name;
	meGPUBuffer vertBuffer = {};
	meGPUBuffer idxBuffer = {};
	meGPUBuffer normBuffer = {};
	meGPUBuffer texcoordBuffer = {};
	BoundingBox meshBounds = {};

	meMaterialID materialHandle = {};

	bool IsLoaded() const { return vertBuffer.IsValid() && idxBuffer.IsValid(); }
};

struct meMeshPool : public meResourcePool<meMesh>
{
	meMeshPool(
		meAllocator* resourceAllocator,
		meAllocator* payloadAllocator) :
	meResourcePool<meMesh>(resourceAllocator, payloadAllocator) {}

	meMeshID Load(
		RendererFrontend* renderer,
		StringView gltfResPath,
		const cgltf_mesh& inMesh);
};


void meMeshInitialize(EngineContext* engine);

meMeshPool& meMeshPoolGet();

// takes cpu-accessible buffers and uploads them to the gpu
// returns a handle to the loaded mesh
meMeshID meMeshLoadFromMemory(
	meSpan vertBuffer,
	meSpan idxBuffer,
	meSpan normBufferOpt = {},
	meSpan texcoordBufferOpt = {});
