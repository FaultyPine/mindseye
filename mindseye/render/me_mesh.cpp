#include "core/me_app.h"
#include "core/me_profile.h"
#include "me_mesh.h"
#include "external/cgltf.h"
#include "asset/me_asset_import_gltf.h"
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


void meMeshInitialize(EngineContext* engine)
{
	engine->meshSystem = MENEW(&engine->engineArena, meMeshPool, &engine->engineArena, &engine->engineArena);
	GenCubeMesh(ME_RESOURCE_INVALID, 2); // Bad data mesh is a cube
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

    virtual bool meAssetImport(OSFileReference importThis) override
    {
        ME_PROFILE_FUNCTION();
        StringView importFileStr = importThis.GetPath();
        if (FindInString(importFileStr, STRING_LIT(".gltf")) != -1 ||
            FindInString(importFileStr, STRING_LIT(".glb")) != -1)
        {
            meAsset newAsset{};
			if (meAssetImportGltfMesh(newAsset, importThis))
            {
                return meAssetWrite(newAsset);
            }
        }
        return false;
    }

    bool meAssetInitializeRuntime(meAsset& asset) override
    {

        meMesh& mesh = *(meMesh*)asset.runtimeContent.get();
        auto upload = [](meGPUBuffer& buffer, u32 layout) {
            if (buffer.cpuData && buffer.bufferHandle == U32_INVALID_ID)
            {
                buffer.bufferHandle = RendererGetMain().CreateVertexBuffer(buffer.cpuData, NTH_BIT(layout));
            }
            return !buffer.cpuData || buffer.bufferHandle != U32_INVALID_ID;
        };
        if (!upload(mesh.vertBuffer, meMeshVertexLayoutType_Position) ||
            !upload(mesh.normBuffer, meMeshVertexLayoutType_Normal) ||
            !upload(mesh.texcoordBuffer, meMeshVertexLayoutType_TexCoord0) || !upload(mesh.idxBuffer, mesh.indexLayout))
        {
            return false;
        }
        if (!mesh.material.runtimeHandle)
        {
            return true;
        }
        mesh.material.runtimeContent = meMaterialGetPool().GetShared(mesh.material.runtimeHandle);
        return meAssetSystemGet().assetLoaders[MAMaterial]->meAssetInitializeRuntime(mesh.material);
    }



    static void RegisterAssetLoader(meEventPayload payload)
    {
        meAllocator* allocator = (meAllocator*)payload.payload;
        meAssetRegisterLoader(MENEW(allocator, meMeshAssetLoader, &TD_MEMESH, &meMeshPoolGet(), meAssetType::MAMesh));
    }
};

MEEVENT_REGISTER_STATIC(registerAssetLoader, meMeshAssetLoader::RegisterAssetLoader);
