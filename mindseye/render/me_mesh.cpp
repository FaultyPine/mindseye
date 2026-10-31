#include "core/me_app.h"
#include "core/me_profile.h"
#include "me_mesh.h"
#include "external/cgltf.h"
#include "render/me_material.h"
#include "core/containers/dynarray.h"
#include "render/renderer_frontend.h"
#include "core/me_math.h"
#include "core/me_scope_exit.h"
#include "core/me_filesystem.h"
#include "platform/me_os.h"

#define PAR_SHAPES_IMPLEMENTATION
#include "external/par_shapes.h"

ME_DEFINE_LOG_CATEGORY(MeshSystem);

#define USE_PAR_SHAPES

static cgltf_material GenerateDummyGLTFMaterial()
{
	cgltf_material mat;
	ME_MEMCLEAR(&mat, sizeof(mat));
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wwritable-strings"
	mat.name = "NoMaterial";
	#pragma clang diagnostic pop
	return mat;
}

void meMeshInitialize(EngineContext* engine)
{
	engine->meshSystem = MENEW(&engine->engineArena, meMeshPool, &engine->engineArena, &engine->engineArena);
	GenCubeMesh(EYE_INVALID, 2); // Bad data mesh is a cube
}

meMeshPool& meMeshPoolGet()
{
	return *GetEngineCtx()->meshSystem;
}



void meMeshPool::Load(
    meMeshID outMeshHandle,
	meSpan vertBuffer,
	meSpan idx16Buffer,
	meSpan normBufferOpt,
	meSpan texcoordBufferOpt,
	meMaterialID materialIDOpt,
	StringView nameOpt)
{
	ME_PROFILE_FUNCTION();
	meMeshPool& meshPool = meMeshPoolGet();
	RendererFrontend& renderer = RendererGetMain();
	meMesh& outMesh = meshPool.Get(outMeshHandle);
	
	outMesh.vertBuffer.cpuData = vertBuffer;
	outMesh.vertBuffer.bufferHandle = renderer.CreateVertexBuffer(outMesh.vertBuffer.cpuData, NTH_BIT(meMeshVertexLayoutType_Position));
	
	if (idx16Buffer)
	{
		outMesh.idxBuffer.cpuData = idx16Buffer;
		outMesh.idxBuffer.bufferHandle = renderer.CreateVertexBuffer(outMesh.idxBuffer.cpuData, NTH_BIT(meMeshVertexLayoutType_Index16));
	}
	
	if (normBufferOpt)
	{
		outMesh.normBuffer.cpuData = normBufferOpt;
		outMesh.normBuffer.bufferHandle = renderer.CreateVertexBuffer(outMesh.normBuffer.cpuData, NTH_BIT(meMeshVertexLayoutType_Normal));
	}
	
	if (texcoordBufferOpt)
	{
		outMesh.texcoordBuffer.cpuData = texcoordBufferOpt;
		outMesh.texcoordBuffer.bufferHandle = renderer.CreateVertexBuffer(outMesh.texcoordBuffer.cpuData, NTH_BIT(meMeshVertexLayoutType_TexCoord0));
	}

	outMesh.material = meTypedAsset<MAMaterial>(materialIDOpt);
	outMesh.name = nameOpt;
}

meMeshID meMeshPool::Load(
	RendererFrontend* renderer,
	StringView gltfResPath,
	const cgltf_mesh& inMesh)
{
	ME_PROFILE_FUNCTION();
	meMaterialPool& materialPool = meMaterialGetPool();
	meMeshPool& meshPool = meMeshPoolGet();
	meMeshID meshHandle = meshPool.Load({.resourceType = meResourceType_InstanceAsset});
	meMesh& outMesh = meshPool.Get(meshHandle);
	meAllocator* meshPayloadAllocator = meshPool.GetPayloadAllocator();

	outMesh.name = String(StringFromCString(inMesh.name), renderer->rendererPersistentAllocator);
	BoundingBox& meshBounds = outMesh.meshBounds;
	for (u64 meshPrimIdx = 0; meshPrimIdx < inMesh.primitives_count; meshPrimIdx++)
	{
		const cgltf_primitive& prim = inMesh.primitives[meshPrimIdx];
		outMesh.material = meTypedAsset<MAMaterial>(materialPool.Load(renderer, gltfResPath, prim.material ? *prim.material : GenerateDummyGLTFMaterial()));
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
			const cgltf_accessor* accessor = prim.indices;
            u64 stride = accessor->stride;
            u64 indicesMemSize = accessor->count * stride;
            Allocation indicesMemory = MEALLOC(meshPayloadAllocator, indicesMemSize);
            u8* indicesBumper = (u8*)indicesMemory.data;
            meMeshVertexLayoutType indexLayoutType = meMeshVertexLayoutType_Index16;
            switch (accessor->component_type)
            {
                case cgltf_component_type_r_16u:
                    indexLayoutType = meMeshVertexLayoutType_Index16;
                    break;
                    case cgltf_component_type_r_32u:
                    indexLayoutType = meMeshVertexLayoutType_Index32;
                    break;
                default:
                    ME_ASSERT(false && "Unsupported index buffer stride");
                    break;
            }
            for (u64 idx = 0; idx < accessor->count; idx++)
            {
                u32 readIdx = (u32)cgltf_accessor_read_index(accessor, idx);
                switch (accessor->component_type)
                {
                    case cgltf_component_type_r_8u:
                        *(u8*)indicesBumper = (u8)readIdx;
                        break;
                    case cgltf_component_type_r_16u:
                        *(u16*)indicesBumper = (u16)readIdx;
                        break;
                    case cgltf_component_type_r_32u:
                        *(u32*)indicesBumper = (u32)readIdx;
                        break;
                    default:
                        // Handle error
                        break;
                }
                indicesBumper += stride;
            }
			outMesh.idxBuffer.cpuData = indicesMemory;
			outMesh.idxBuffer.bufferHandle = renderer->CreateVertexBuffer(outMesh.idxBuffer.cpuData, NTH_BIT(indexLayoutType));
		}
	}
	return meshHandle;
}


static void LoadGeneratedMesh(meMeshID handle, par_shapes_mesh* shape,
    meMaterialID material, StringView name)
{
    ME_ASSERT(shape);
    ME_ON_SCOPE_EXIT([shape]() { par_shapes_free_mesh(shape); });
    if (!shape->normals)
    {
        par_shapes_compute_normals(shape);
    }

    meMeshPool& pool = meMeshPoolGet();
    meAllocator* allocator = pool.GetPayloadAllocator();
    Allocation positions = MEALLOC(allocator, shape->npoints * 3 * sizeof(float));
    Allocation normals = MEALLOC(allocator, shape->npoints * 3 * sizeof(float));
    Allocation uvs = MEALLOC(allocator, shape->npoints * 2 * sizeof(float));
    Allocation indices = MEALLOC(allocator, shape->ntriangles * 3 * sizeof(u16));
    static_assert(sizeof(PAR_SHAPES_T) == sizeof(u16));
    ME_MEMCPY(positions.data, shape->points, positions.size);
    ME_MEMCPY(normals.data, shape->normals, normals.size);
    ME_MEMCPY(indices.data, shape->triangles, indices.size);
    if (shape->tcoords)
    {
        ME_MEMCPY(uvs.data, shape->tcoords, uvs.size);
    }
    else
    {
        ME_MEMCLEAR(uvs.data, uvs.size);
    }

    float bounds[6];
    par_shapes_compute_aabb(shape, bounds);
    pool.Load(handle, positions, indices, normals, uvs, material, name);
    pool.Get(handle).meshBounds = BoundingBox(glm::make_vec3(bounds), glm::make_vec3(bounds + 3));
}

void GenCubeMesh(meMeshID handle, u32 resolution, meMaterialID material)
{
    par_shapes_mesh* shape = par_shapes_create_cube();
    // cube center from (0.5, 0.5) -> (0, 0)
    par_shapes_translate(shape, -0.5f, -0.5, -0.5f);
    LoadGeneratedMesh(handle, shape, material, STRING_LIT("GeneratedCubeMesh"));
}

void GenPlaneMesh(meMeshID handle, u32 resolution, meMaterialID material)
{
    s32 segments = (s32)glm::clamp(resolution, 1u, 128u);
    par_shapes_mesh* shape = par_shapes_create_plane(segments, segments);
    const float axis[] = {1, 0, 0};
    par_shapes_rotate(shape, -PI / 2, axis);
    par_shapes_translate(shape, -0.5f, 0, 0.5f);
    LoadGeneratedMesh(handle, shape, material, STRING_LIT("GeneratedPlaneMesh"));
}

void GenSphereMesh(meMeshID handle, u32 resolution, meMaterialID material)
{
    s32 segments = (s32)glm::clamp(resolution, 3u, 128u);
    LoadGeneratedMesh(handle, par_shapes_create_parametric_sphere(segments, segments),
        material, STRING_LIT("GeneratedSphereMesh"));
}

void GenCylinderMesh(meMeshID handle, u32 resolution, meMaterialID material)
{
    s32 segments = (s32)glm::clamp(resolution, 3u, 128u);
    par_shapes_mesh* shape = par_shapes_create_cylinder(segments, 1);
    // cylinder is initially z-up, radius 1, and extends from 0 to 1
    const float bottom[] = {0, 0, 0};
    const float top[] = {0, 0, 1};
    const float up[] = {0, 0, 1};
    par_shapes_mesh* caps[] = 
    {
        par_shapes_create_disk(1, segments, bottom, up),
        par_shapes_create_disk(1, segments, top, up)
    };
    // both bottom and top caps are created facing up, so explicitly flip the bottom one 180deg around X to face down
    // just passing a down axis to par_shapes_create_disk doesn't cut it. It uses a cross product with a hardcoded {0,0,-1} which produces the wrong result
    const float capAxis[] = {1, 0, 0};
    par_shapes_rotate(caps[0], PI, capAxis);

    for (par_shapes_mesh* cap : caps)
    {
        cap->tcoords = PAR_MALLOC(float, cap->npoints * 2);
        for (s32 i = 0; i < cap->npoints; ++i)
        {
            cap->tcoords[i * 2] = cap->points[i * 3] * 0.5f + 0.5f;
            cap->tcoords[i * 2 + 1] = cap->points[i * 3 + 1] * 0.5f + 0.5f;
        }
        par_shapes_merge_and_free(shape, cap);
    }
    const float axis[] = {1, 0, 0};
    par_shapes_rotate(shape, -PI / 2, axis);
    par_shapes_translate(shape, 0, -0.5f, 0);
    LoadGeneratedMesh(handle, shape, material, STRING_LIT("GeneratedCylinderMesh"));
}

struct meMeshAssetLoader : public meAssetLoader
{
	using meAssetLoader::meAssetLoader;

	virtual void meAssetLoad(meAsset& asset) override
	{
		ME_PROFILE_FUNCTION();
		meAssetLoader::meAssetLoad(asset);
		meAllocator* allocator = resourcePool->GetPayloadAllocator();
        ScopedAssetLockR<meMesh> outMesh(asset);
		if (FindInString(outMesh->externalMeshPath, STRING_LIT(".gltf")) != -1 ||
			FindInString(outMesh->externalMeshPath, STRING_LIT(".glb")) != -1)
		{
            StringView resourcePathAbs = meAssetGetAbsPathForResource(outMesh->externalMeshPath);
            OSFileReference file;
            meOSOpenFile(file, resourcePathAbs, (OSFileFlags_OnlyIfExists | OSFileFlags_ScopedFile | OSFileFlags_ReadOnly));
            u64 filesize = meOSGetFileSize(file);
            Allocation gltfBuffer = MEALLOC(allocator, filesize);
            if (!meOSReadFileContents(file, gltfBuffer.data, gltfBuffer.size))
            {
                LOG_ERROR(MeshSystem, "[meScene] failed to load gltf scene {}", LogSV(resourcePathAbs));
            }
            cgltf_options options = {};
            cgltf_data* gltfData = nullptr;

            ME_ON_SCOPE_EXIT([allocator, gltfData, &gltfBuffer]()
            {
                cgltf_free(gltfData);
                MEFREE(allocator, gltfBuffer);
            });
            cgltf_result parseResult = {};
			{
				ME_PROFILE_SCOPE("cgltf_parse mesh");
				parseResult = cgltf_parse(&options, gltfBuffer.data, gltfBuffer.size, &gltfData);
			}
            if (parseResult == cgltf_result_success)
            {
				ME_PROFILE_SCOPE("cgltf_load_buffers mesh");
                parseResult = cgltf_load_buffers(&options, gltfData, resourcePathAbs.cstr());
                if (parseResult != cgltf_result_success)
                {
                    LOG_WARN(MeshSystem, "Failed to load gltf buffers from {}", LogSV(resourcePathAbs));
                }
            }
            else
            {
                LOG_WARN(MeshSystem, "Failed to parse gltf from {}", LogSV(resourcePathAbs));
            }
            ME_ASSERT(gltfData->meshes_count);
            if (gltfData->meshes_count > 1)
            {
                LOG_WARN(MeshSystem, "Loading a gltf mesh which contains multiple meshes will ignore all but the first mesh");
            }
            StringView gltfResPath = msFsGetDirFromPath(resourcePathAbs);
			meMeshPoolGet().Load(GetEngineCtx()->renderer, gltfResPath, gltfData->meshes[0]);
		}
	}

	static void RegisterAssetLoader(meEventPayload payload)
	{
		meAllocator* allocator = (meAllocator*)payload.payload;
		meAssetRegisterLoader(MENEW(allocator, meMeshAssetLoader, &TD_MEMESH, &meMeshPoolGet(), meAssetType::MAMesh));
	}
};

MEEVENT_REGISTER_STATIC(registerAssetLoader, meMeshAssetLoader::RegisterAssetLoader);
