#pragma once

#include "core/me_core.h"
#include "me_gpu.h"
#include "core/me_resourcepool.h"
#include "render/me_material.h"

struct cgltf_mesh;

typedef Eye meMeshID;

typedef u32 meMeshVertexLayoutType;
enum meMeshVertexLayoutType_
{
	meMeshVertexLayoutType_Index16,
	meMeshVertexLayoutType_Index32,
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

// mostly used for runtime-generated meshes
// not loaded meshes
struct meFatVertex
{
	glm::vec3 position = glm::vec3(0);
    glm::vec3 normal = glm::vec3(0);
    glm::vec3 tangent = glm::vec3(0);
    glm::vec2 texCoords = glm::vec3(0);
    glm::vec4 color = glm::vec4(1);
    u32 objectID = U32_INVALID_ID;
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

	meMeshID Load(
		meSpan vertBuffer,
		meSpan idx16Buffer,
		meSpan normBufferOpt = {},
		meSpan texcoordBufferOpt = {},
		meMaterialID materialIDOpt = {},
		StringView nameOpt = {});
};


void meMeshInitialize(EngineContext* engine);

meMeshPool& meMeshPoolGet();

MEAPI meMeshID GenSphereMesh(u32 resolution);
MEAPI meMeshID GenPlaneMesh(u32 resolution);
