#include "me_mesh.h"
#include "external/cgltf.h"
#include "render/me_material.h"
#include "core/containers/dynarray.h"
#include "render/renderer_frontend.h"
#include "core/me_math.h"
#include "core/me_scope_exit.h"
#include "core/me_profile.h"
#include "core/me_filesystem.h"
#include "core/me_command.h"
#include "platform/me_os.h"

#define PAR_SHAPES_IMPLEMENTATION
#include "external/par_shapes.h"

#define USE_PAR_SHAPES

static cgltf_material GenerateDummyGLTFMaterial()
{
	cgltf_material mat;
	ME_MEMCLEAR(&mat, sizeof(mat));
	static char name[] = "NoMaterial";
	mat.name = name;
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

static void QueueMainThreadMeshBufferUpload(
	meMeshID meshHandle,
	meSpan cpuData,
	meMeshVertexLayoutType layout)
{
	if (!cpuData)
	{
		return;
	}

	meExternalCommand cmd = {};
	cmd.type = meExternalCommandType_MainThreadCmd;
	cmd.mainThreadCmd.fn = [meshHandle, cpuData, layout]()
	{
		meMesh& mesh = meMeshPoolGet().Get(meshHandle);
		u32 bufferHandle = (u32)RendererGetMain().CreateVertexBuffer(cpuData, layout);
		if (TEST_BIT(layout, meMeshVertexLayoutType_Index16) || TEST_BIT(layout, meMeshVertexLayoutType_Index32))
		{
			mesh.idxBuffer.bufferHandle = bufferHandle;
		}
		else if (TEST_BIT(layout, meMeshVertexLayoutType_Position))
		{
			mesh.vertBuffer.bufferHandle = bufferHandle;
		}
		else if (TEST_BIT(layout, meMeshVertexLayoutType_Normal))
		{
			mesh.normBuffer.bufferHandle = bufferHandle;
		}
		else if (TEST_BIT(layout, meMeshVertexLayoutType_TexCoord0))
		{
			mesh.texcoordBuffer.bufferHandle = bufferHandle;
		}
	};
	meSendExternalCommand(cmd);
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
				QueueMainThreadMeshBufferUpload(meshHandle, outMesh.vertBuffer.cpuData, NTH_BIT(meMeshVertexLayoutType_Position));
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
				QueueMainThreadMeshBufferUpload(meshHandle, outMesh.normBuffer.cpuData, NTH_BIT(meMeshVertexLayoutType_Normal));
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
				QueueMainThreadMeshBufferUpload(meshHandle, outMesh.texcoordBuffer.cpuData, NTH_BIT(meMeshVertexLayoutType_TexCoord0));
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
			QueueMainThreadMeshBufferUpload(meshHandle, outMesh.idxBuffer.cpuData, NTH_BIT(indexLayoutType));
		}
	}
	return meshHandle;
}


MEAPI void GenCubeMesh(
    meMeshID outMeshHandle,
    u32 resolution, 
    meMaterialID materialID)
{
	meMeshPool& meshPool = meMeshPoolGet();

    par_shapes_mesh* cube = par_shapes_create_cube();
    ME_ON_SCOPE_EXIT([cube](){
		par_shapes_free_mesh(cube);
	});
    //par_shapes_compute_normals(cube);
    meAllocator* allocator = meshPool.GetPayloadAllocator();
	Allocation verticesData = MEALLOC(allocator, cube->ntriangles * 3 * 3 * sizeof(float));
	Allocation texcoordData = MEALLOC(allocator, cube->ntriangles * 3 * 2 * sizeof(float));
	Allocation normalsData = MEALLOC(allocator, cube->ntriangles * 3 * 3 * sizeof(float));
	s32 vertexCount = cube->ntriangles * 3;

	for (int k = 0; k < vertexCount; k++)
    {
        ((float*)verticesData)[k*3] = cube->points[cube->triangles[k]*3];
        ((float*)verticesData)[k*3 + 1] = cube->points[cube->triangles[k]*3 + 1];
        ((float*)verticesData)[k*3 + 2] = cube->points[cube->triangles[k]*3 + 2];

        ((float*)normalsData)[k*3] = cube->normals[cube->triangles[k]*3];
        ((float*)normalsData)[k*3 + 1] = cube->normals[cube->triangles[k]*3 + 1];
        ((float*)normalsData)[k*3 + 2] = cube->normals[cube->triangles[k]*3 + 2];

        ((float*)texcoordData)[k*2] = cube->tcoords[cube->triangles[k]*2];
        ((float*)texcoordData)[k*2 + 1] = cube->tcoords[cube->triangles[k]*2 + 1];
    }

	float aabb[6];
	par_shapes_compute_aabb(cube, aabb);

	meshPool.Load(outMeshHandle, verticesData, {}, normalsData, texcoordData, materialID, STRING_LIT("GeneratedCubeMesh"));
	meMesh& outMesh = meshPool.Get(outMeshHandle);
	outMesh.meshBounds = BoundingBox(glm::vec3(aabb[0], aabb[1], aabb[2]), glm::vec3(aabb[3], aabb[4], aabb[5]));
}

void GenPlaneMesh(
    meMeshID outMeshHandle,
    u32 resolution,
    meMaterialID materialID) 
{
	meMeshPool& meshPool = meMeshPoolGet();

	s32 resX = resolution;
	s32 resZ = resolution;
	float length = 1.0f;
	float width = 1.0f;
	par_shapes_mesh* plane = par_shapes_create_plane(resX, resZ);   // No normals/texcoords generated!!!
    par_shapes_scale(plane, width, length, 1.0f);
    par_shapes_translate(plane, -width/2, 0.0f, length/2);

	ME_ON_SCOPE_EXIT([plane](){
		par_shapes_free_mesh(plane);
	});
	meAllocator* allocator = meshPool.GetPayloadAllocator();
	Allocation verticesData = MEALLOC(allocator, plane->ntriangles * 3 * 3 * sizeof(float));
	Allocation texcoordData = MEALLOC(allocator, plane->ntriangles * 3 * 2 * sizeof(float));
	Allocation normalsData = MEALLOC(allocator, plane->ntriangles * 3 * 3 * sizeof(float));
	s32 vertexCount = plane->ntriangles * 3;

	for (int k = 0; k < vertexCount; k++)
    {
        ((float*)verticesData)[k*3] = plane->points[plane->triangles[k]*3];
        ((float*)verticesData)[k*3 + 1] = plane->points[plane->triangles[k]*3 + 1];
        ((float*)verticesData)[k*3 + 2] = plane->points[plane->triangles[k]*3 + 2];

        ((float*)normalsData)[k*3] = plane->normals[plane->triangles[k]*3];
        ((float*)normalsData)[k*3 + 1] = plane->normals[plane->triangles[k]*3 + 1];
        ((float*)normalsData)[k*3 + 2] = plane->normals[plane->triangles[k]*3 + 2];

        ((float*)texcoordData)[k*2] = plane->tcoords[plane->triangles[k]*2];
        ((float*)texcoordData)[k*2 + 1] = plane->tcoords[plane->triangles[k]*2 + 1];
    }

	float aabb[6];
	par_shapes_compute_aabb(plane, aabb);

	meshPool.Load(outMeshHandle, verticesData, {}, normalsData, texcoordData, materialID, STRING_LIT("GeneratedPlaneMesh"));
	meMesh& outMesh = meshPool.Get(outMeshHandle);
	outMesh.meshBounds = BoundingBox(glm::vec3(aabb[0], aabb[1], aabb[2]), glm::vec3(aabb[3], aabb[4], aabb[5]));
}


void GenSphereMesh(
    meMeshID outMeshHandle,
    u32 resolution,
    meMaterialID materialID)
{
	meMeshPool& meshPool = meMeshPoolGet();
    f32 radius = 1.0f;
    u32 stackCount = resolution;
    u32 sectorCount = resolution;
    DynArray<meFatVertex> vertices = DynArrayCreate<meFatVertex>(meshPool.GetPayloadAllocator(), stackCount * sectorCount);
    DynArray<u32> indices = DynArrayCreate<u32>(meshPool.GetPayloadAllocator(), stackCount * sectorCount);

    float x, y, z, xy;                              // vertex position
    float nx, ny, nz, lengthInv = 1.0f / radius;    // vertex normal
    float s, t;                                     // vertex texCoord

    float sectorStep = 2 * PI / sectorCount;
    float stackStep = PI / stackCount;
    float sectorAngle, stackAngle;

    for(u32 i = 0; i <= stackCount; ++i)
    {
        stackAngle = PI / 2 - i * stackStep;        // starting from pi/2 to -pi/2
        xy = radius * cosf(stackAngle);             // r * cos(u)
        z = radius * sinf(stackAngle);              // r * sin(u)

        // add (sectorCount+1) vertices per stack
        // first and last vertices have same position and normal, but different tex coords
        for(u32 j = 0; j <= sectorCount; ++j)
        {
            sectorAngle = j * sectorStep;           // starting from 0 to 2pi
            meFatVertex v = {};
            // vertex position (x, y, z)
            x = xy * cosf(sectorAngle);             // r * cos(u) * cos(v)
            y = xy * sinf(sectorAngle);             // r * cos(u) * sin(v)
            glm::vec3 vertexPosition = glm::vec3(x,y,z);
            v.position = vertexPosition;

            // normalized vertex normal (nx, ny, nz)
            nx = x * lengthInv;
            ny = y * lengthInv;
            nz = z * lengthInv;
            glm::vec3 normal = glm::vec3(nx, ny, nz);
            v.normal = normal;

            // vertex tex coord (s, t) range between [0, 1]
            s = (float)j / sectorCount;
            t = (float)i / stackCount;
            glm::vec2 texcoord = glm::vec2(s,t);
            v.texCoords = texcoord;

            DynArrayPush(vertices, v);
        }
    }

    u32 k1, k2;
    for(u32 i = 0; i < stackCount; ++i)
    {
        k1 = i * (sectorCount + 1);     // beginning of current stack
        k2 = k1 + sectorCount + 1;      // beginning of next stack

        for(u32 j = 0; j < sectorCount; ++j, ++k1, ++k2)
        {
            // 2 triangles per sector excluding first and last stacks
            // k1 => k2 => k1+1
            if(i != 0)
            {
                DynArrayPush(indices, k1);
				DynArrayPush(indices, k2);
				DynArrayPush(indices, k1 + 1);
            }

            // k1+1 => k2 => k2+1
            if(i != (stackCount-1))
            {
                DynArrayPush(indices, k1 + 1);
				DynArrayPush(indices, k2);
				DynArrayPush(indices, k2 + 1);
            }
        }
    }
    //vertices.shrink_to_fit();
    //indices.shrink_to_fit();
	meSpan vertexSpan = meSpan((s8*)vertices.data, DynArrayGetSize(vertices) * sizeof(*vertices));
	meSpan indexSpan = meSpan((s8*)indices.data, DynArrayGetSize(indices) * sizeof(*indices));
    meshPool.Load(outMeshHandle, vertexSpan, indexSpan, {}, {}, materialID, STRING_LIT("GeneratedSphereMesh"));
	meMesh& outMesh = meshPool.Get(outMeshHandle);
	outMesh.meshBounds = BoundingBox(glm::vec3(-radius, -radius, -radius), glm::vec3(radius, radius, radius));
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
                LOG_ERROR("[meScene] failed to load gltf scene %.*s", STRING_VAARGS(resourcePathAbs));
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
                    LOG_WARN("Failed to load gltf buffers from %.*s", STRING_VAARGS(resourcePathAbs));
                }
            }
            else
            {
                LOG_WARN("Failed to parse gltf from %.*s", STRING_VAARGS(resourcePathAbs));
            }
            ME_ASSERT(gltfData->meshes_count);
            if (gltfData->meshes_count > 1)
            {
                LOG_WARN("Loading a gltf mesh which contains multiple meshes will ignore all but the first mesh");
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
