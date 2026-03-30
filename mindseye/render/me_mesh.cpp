#include "me_mesh.h"
#include "external/cgltf.h"
#include "render/me_material.h"
#include "core/containers/dynarray.h"
#include "render/renderer_frontend.h"
#include "core/me_math.h"
#include "core/me_scope_exit.h"

#define PAR_SHAPES_IMPLEMENTATION
#include "external/par_shapes.h"

#define USE_PAR_SHAPES

void meMeshInitialize(EngineContext* engine)
{
	engine->meshSystem = MENEW(&engine->engineArena, meMeshPool, &engine->engineArena, &engine->engineArena);
	meMesh defaultBadDataMesh = {};
	defaultBadDataMesh.name = StringFromCString("BadDataMesh");
	// TODO: cube?
	meMeshID defaultMesh = GenPlaneMesh(2);
	// we technically are "leaking" the default mesh id here.
	engine->meshSystem->GetBadData() = meMeshPoolGet().Get(defaultMesh);
    meAssetRegisterRuntime({}, MAMesh);
}

meMeshPool& meMeshPoolGet()
{
	return *GetEngineCtx()->meshSystem;
}



meMeshID meMeshPool::Load(
	meSpan vertBuffer,
	meSpan idx16Buffer,
	meSpan normBufferOpt,
	meSpan texcoordBufferOpt,
	meMaterialID materialIDOpt,
	StringView nameOpt)
{
	meMeshPool& meshPool = meMeshPoolGet();
	RendererFrontend& renderer = RendererGetMain();
	meMeshID meshHandle = meshPool.CreateInternal();
	meMesh& outMesh = meshPool.Get(meshHandle);
	
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

	outMesh.materialHandle = materialIDOpt;
	outMesh.name = nameOpt;

	return meshHandle;
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
		outMesh.materialHandle = materialPool.Load(renderer, gltfResPath, prim.material ? *prim.material : GenerateDummyGLTFMaterial());
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


meMeshID GenPlaneMesh(
    u32 resolution,
    meMaterialID materialID) 
{
	meMeshPool& meshPool = meMeshPoolGet();

#ifdef USE_PAR_SHAPES
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

	meMeshID meshHandle = meshPool.Load(verticesData, {}, normalsData, texcoordData, materialID, STRING_LIT("GeneratedPlaneMesh"));
	return meshHandle;

#else
    resolution++; // resolution of 1 should really be 2
	s32 vertexCount = resolution * resolution;
    DynArray<glm::vec3> planeverts = DynArrayCreateWithReserved<glm::vec3>(meshPool.GetPayloadAllocator(), vertexCount);

    // https://github.com/raysan5/raylib/blob/master/src/rmodels.c#L2171
    for (u32 z = 0; z < resolution; z++) {
        // [-length/2, length/2]
        f32 yPos = ((f32)z/(resolution - 1) - 0.5f) * length;
        for (u32 x = 0; x < resolution; x++) {
            // [-width/2, width/2]
            f32 xPos = ((f32)x/(resolution - 1) - 0.5f) * width;
            glm::vec3 v = glm::vec3(xPos, yPos, 0.0f);
			planeverts[x + z * resolution] = v;
        }
    }

    u32 numFaces = (resolution - 1)*(resolution - 1);
    DynArray<u32> indices = DynArrayCreateWithReserved<u32>(meshPool.GetPayloadAllocator(), numFaces * 6);
	s32 t = 0;
    for (u32 face = 0; face < numFaces; face++) 
	{
        // Retrieve lower left corner from face ind
        u32 i = face + face/(resolution - 1);

		indices[t++] = i + resolution;
		indices[t++] = i + 1;
		indices[t++] = i;

		indices[t++] = i + resolution;
		indices[t++] = i + resolution + 1;
		indices[t++] = i + 1;
    }

	meSpan vertexBufferSpan = meSpan((s8*)planeverts.data, DynArrayGetSize(planeverts) * sizeof(glm::vec3));
	meSpan indexBufferSpan = meSpan((s8*)indices.data, DynArrayGetSize(indices) * sizeof(u32));
	meMeshID meshHandle = meshPool.Load(vertexBufferSpan, indexBufferSpan, {}, {}, materialID, STRING_LIT("GeneratedPlaneMesh"));
	return meshHandle;
#endif
}


meMeshID GenSphereMesh(
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
    meMeshID result = meshPool.Load(vertexSpan, indexSpan, {}, {}, materialID, STRING_LIT("GeneratedSphereMesh"));
	return result;
}
