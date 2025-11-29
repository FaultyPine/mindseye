#include "me_mesh.h"
#include "external/cgltf.h"
#include "render/me_material.h"

void meMeshInitialize(EngineContext* engine)
{
	engine->meshSystem = MENEW(&engine->engineArena, meMeshPool, &engine->engineArena, &engine->engineArena);
	meMesh defaultBadDataMesh = {};
	defaultBadDataMesh.name = StringFromCString("BadDataMesh");
	// TODO: cube?
	engine->meshSystem->badData = defaultBadDataMesh;
}

meMeshPool& meMeshPoolGet()
{
	return *GetEngineCtx()->meshSystem;
}


meMeshID meMeshLoadFromMemory(
	meSpan vertBuffer,
	meSpan idxBuffer,
	meSpan normBufferOpt,
	meSpan texcoordBufferOpt)
{
	ME_ASSERT(false);
	return {};
}

meMeshID meMeshPool::Load(
	RendererFrontend* renderer,
	StringView gltfResPath,
	const cgltf_mesh& inMesh)
{
	meMaterialPool& materialPool = meMaterialGetPool();
	meMeshPool& meshPool = meMeshPoolGet();
	meMeshID meshHandle = meshPool.CreateInternal();
	meMesh& outMesh = meshPool.Get(meshHandle);
	meAllocator* meshPayloadAllocator = meshPool.GetPayloadAllocator();

	outMesh.name = String(StringFromCString(inMesh.name), renderer->rendererPersistentAllocator);
	BoundingBox& meshBounds = outMesh.meshBounds;
	for (u64 meshPrimIdx = 0; meshPrimIdx < inMesh.primitives_count; meshPrimIdx++)
	{
		const cgltf_primitive& prim = inMesh.primitives[meshPrimIdx];
		outMesh.materialHandle = materialPool.Load(renderer, gltfResPath, prim.material ? *prim.material : GenerateDummyMaterial());
		// attribs like position, texcoords, normals
		for (u64 attributeIdx = 0; attributeIdx < prim.attributes_count; attributeIdx++)
		{
			const cgltf_attribute& attrib = prim.attributes[attributeIdx];
			StringView attribName = StringView(attrib.name, CStringLength(attrib.name));
			if (attrib.type == cgltf_attribute_type_position)
			{
				const cgltf_accessor* accessor = attrib.data;
				u64 stride = sizeof(f32) * 3;
				u64 dataSize = stride * accessor->count;
				Allocation allocation = MEALLOC(meshPayloadAllocator, dataSize);
				Allocation bumper = allocation;
				for (u64 posIdx = 0; posIdx < accessor->count; posIdx++)
				{
					cgltf_accessor_read_float(accessor, posIdx, bumper, 3);
					meshBounds.min = glm::min(meshBounds.min, glm::make_vec3((float*)bumper.data));
					meshBounds.max = glm::max(meshBounds.max, glm::make_vec3((float*)bumper.data));
					bumper = bumper.Subspan(stride);
				}
				outMesh.vertBuffer.cpuData = allocation;
				outMesh.vertBuffer.bufferHandle = renderer->CreateVertexBuffer(outMesh.vertBuffer.cpuData, NTH_BIT(meMeshVertexLayoutType_Position));
			}
			else if (attrib.type == cgltf_attribute_type_normal)
			{
				const cgltf_accessor* accessor = attrib.data;
				u64 stride = sizeof(f32) * 3;
				u64 dataSize = stride * accessor->count;
				Allocation allocation = MEALLOC(meshPayloadAllocator, dataSize);
				Allocation bumper = allocation;
				for (u64 posIdx = 0; posIdx < accessor->count; posIdx++)
				{
					cgltf_accessor_read_float(accessor, posIdx, bumper, 3);
					bumper = bumper.Subspan(stride);
				}
				outMesh.normBuffer.cpuData = allocation;
				outMesh.normBuffer.bufferHandle = renderer->CreateVertexBuffer(outMesh.normBuffer.cpuData, NTH_BIT(meMeshVertexLayoutType_Normal));
			}
			else if (attrib.type == cgltf_attribute_type_tangent)
			{

			}
			else if (attrib.type == cgltf_attribute_type_texcoord)
			{
				const cgltf_accessor* accessor = attrib.data;
				u64 stride = sizeof(f32) * 2;
				u64 dataSize = stride * accessor->count;
				Allocation allocation = MEALLOC(meshPayloadAllocator, dataSize);
				Allocation bumper = allocation;
				for (u64 posIdx = 0; posIdx < accessor->count; posIdx++)
				{
					cgltf_accessor_read_float(accessor, posIdx, bumper, 2);
					bumper = bumper.Subspan(stride);
				}
				outMesh.texcoordBuffer.cpuData = allocation;
				outMesh.texcoordBuffer.bufferHandle = renderer->CreateVertexBuffer(outMesh.texcoordBuffer.cpuData, NTH_BIT(meMeshVertexLayoutType_TexCoord0));
			}
		}

		// indices
		if (prim.indices)
		{
			u64 stride = prim.indices->stride;
			u64 indicesMemSize = prim.indices->count * stride;
			Allocation indicesMemory = MEALLOC(meshPayloadAllocator, indicesMemSize);
			meSpan indicesBumper = indicesMemory;
			for (u64 idx = 0; idx < prim.indices->count; idx++)
			{
				u64 readIdx = cgltf_accessor_read_index(prim.indices, idx);
				ME_MEMCPY(indicesBumper.data, &readIdx, stride);
				indicesBumper = indicesBumper.Subspan(stride);
			}
			outMesh.idxBuffer.cpuData = indicesMemory;
			outMesh.idxBuffer.bufferHandle = renderer->CreateVertexBuffer(outMesh.idxBuffer.cpuData, NTH_BIT(meMeshVertexLayoutType_Index));
		}
	}
	return meshHandle;
}
