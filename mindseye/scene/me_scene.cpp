#include "me_scene.h"

#define CGLTF_IMPLEMENTATION
#include "external/cgltf.h"
#include "asset/me_asset.h"
#include "core/me_scope_exit.h"
#include "core/me_log.h"
#include "core/me_serialize.h"
#include "scene/me_entity.h"
#include "render/me_mesh.h"
#include "platform/me_os.h"

void meSceneInitialize(EngineContext* engine)
{
	engine->sceneSystem = MENEW(&engine->engineArena, meSceneManager);
	engine->scenePool = MENEW(&engine->engineArena, meScenePool, &engine->engineArena, &engine->engineArena);
}

void meSceneInitializeLate(EngineContext* engine)
{
	// start with a "blank" new scene
	engine->sceneSystem->rootScene = meAssetCreateNewInstanceAsset(MAScene);
	// CLEANUP: we're "leaking" this first blank scene, but who cares
}

meScenePool& meScenePoolGet()
{
	return *GetEngineCtx()->scenePool;
}

void meSceneManager::Tick(EngineContext* ctx)
{
	CurrentScene().mainCamera.UpdateCameraWithUserInput(*ctx->osData);
}

void meSceneManager::UnloadCurrentScene()
{
    EngineContext* ctx = GetEngineCtx();

    meAssetUnloadBlocking(SPAN_FROM_TYPED_SINGLE(rootScene));
	rootScene = {};

    meAllocator* sceneAllocator = &ctx->engineSceneAllocator;
	sceneAllocator->meClear();
}

void meSceneManager::ChangeCurrentScene(StringView filename)
{
	UnloadCurrentScene();
	MAID sceneIdent = meAssetIndexGetMAIDFromPath(filename);
    if (!sceneIdent)
    {
        LOG_WARN("Failed to change scene. Scene file " STRING_FMT " doesn't map to a asset", STRING_VAARGS(filename));
        return;
    }
	meAssetRequestLoad(&sceneIdent, 1);
    if (meAsset* asset = meAssetTryGet(sceneIdent))
    {
        GetEngineCtx()->sceneSystem->rootScene = *asset;
        meEventPayload payload = {asset};
        GetEngineCtx()->appCallbacks.onSceneLoaded(payload);
    }
}

void meSceneManager::CopyToRenderInput(meScene& outScene)
{
	// copy the "current"? scene to the given scene for the renderer to use as its readonly copy. This will become complex later...
    // TODO: deep copy everything in the scene - for assets, that means incrementing reader (ref) count
	outScene = CurrentScene();
}

// TODO: BVH
meSceneRaycastHit meSceneRaycast(meScene& scene, const meRay& ray)
{
	meSceneRaycastHit result = {};
	f32 closestT = FLT_MAX;

	DynArray<meAsset>& entities = scene.entities;
	for (DynArray_Foreach(entities, i))
	{
		EntityRef entRef = entities[i];
		meEntity& entData = meEntityGet(entRef);
		if (meEntityIsFlag(entRef, EntityFlags_HIDDEN) || meEntityIsFlag(entRef, EntityFlags_DISABLED))
			continue;

		glm::vec3 worldMin = entData.transform.position + entData.authoritativeBounds.min;
		glm::vec3 worldMax = entData.transform.position + entData.authoritativeBounds.max;

		f32 t = 0.0f;
		if (meRayIntersectsAABB(ray, worldMin, worldMax, t))
		{
			if (t < closestT)
			{
				closestT = t;
				result.entity = entRef;
				result.distance = t;
				result.point = ray.origin + ray.direction * t;
			}
		}
	}

	return result;
}

void meScene::Destroy()
{
    DynArrayDestroy(entities);
}

meScene& meSceneManager::CurrentScene()
{
	meScenePool& scenePool = meScenePoolGet();
	Eye currentSceneHandle = rootScene;
	meScene& result = scenePool.Get(currentSceneHandle);
	return result;
}

struct meSceneAssetLoader : public meAssetLoader
{
	using meAssetLoader::meAssetLoader;

	virtual void meAssetLoad(meAsset& asset) override
	{
		meAssetLoader::meAssetLoad(asset);
		meAllocator* allocator = resourcePool->GetPayloadAllocator();
		meScene& outScene = *(meScene*)resourcePool->GetOpaque(asset.runtimeHandle);
		// if (!outScene.entities)
		// {
		// 	outScene.entities = DynArrayCreate<meAsset>(allocator);
		// }
		if (FindInString(outScene.externalScenePath, STRING_LIT(".gltf")) != -1 ||
			FindInString(outScene.externalScenePath, STRING_LIT(".glb")) != -1)
		{
			meScenePoolGet().Load(allocator, outScene.externalScenePath, outScene);
		}
	}

	static void RegisterAssetLoader(meEventPayload payload)
	{
		meAllocator* allocator = (meAllocator*)payload.payload;
		meAssetRegisterLoader(MENEW(allocator, meSceneAssetLoader, &TD_MESCENE, &meScenePoolGet(), meAssetType::MAScene));
	}
};

MEEVENT_REGISTER_STATIC(registerAssetLoader, meSceneAssetLoader::RegisterAssetLoader);

void meScenePool::Load(
	meAllocator* sceneAllocator,
	StringView resourcePathRel,
	meScene& outScene)
{
	StringView resourcePathAbs = meAssetGetAbsPathForResource(resourcePathRel);
	OSFileReference file;
    meOSOpenFile(file, resourcePathAbs, (OSFileFlags_OnlyIfExists | OSFileFlags_ScopedFile));
	u64 filesize = meOSGetFileSize(file);
	Allocation gltfBuffer = MEALLOC(sceneAllocator, filesize);

    if (!meOSReadFileContents(file, gltfBuffer.data, gltfBuffer.size))
    {
        LOG_ERROR("[meScene] failed to load gltf scene %.*s", STRING_VAARGS(resourcePathAbs));
    }
    cgltf_options options = {};
    cgltf_data* gltfData = nullptr;

    ME_ON_SCOPE_EXIT([gltfData]()
	{
		cgltf_free(gltfData);
	});
	// parses the gltf json metadata
    cgltf_result parseResult = cgltf_parse(&options, gltfBuffer.data, gltfBuffer.size, &gltfData);
    if (parseResult == cgltf_result_success)
    {
		// loads the external buffers (actual geo, textures, etc)
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
	const cgltf_scene& scene = *gltfData->scene;
	DynArray<meAsset>& entities = outScene.entities;
	StringView gltfResPath = msFsGetDirFromPath(resourcePathAbs);
	meMeshPool& meshPool = meMeshPoolGet();
	for (u64 nodeIdx = 0; nodeIdx < scene.nodes_count; nodeIdx++)
	{
		const cgltf_node& node = *scene.nodes[nodeIdx];
		float nodeMatrix[16];
		cgltf_node_transform_local(&node, nodeMatrix);
		meTransform nodeTf = meTransform(glm::make_mat4(nodeMatrix));
		meAsset entityAsset = meEntityCreateBlankInstance(StringFromCString(node.name));
		meEntity& entity = meEntityGet(entityAsset);
		entity.transform = nodeTf;
		if (node.mesh)
		{
			const cgltf_mesh& gltfmesh = *node.mesh;
			meMeshID meshHandle = meshPool.Load(GetEngineCtx()->renderer, gltfResPath, gltfmesh);
			meMesh& mesh = meshPool.Get(meshHandle);
            // TODO: right now, we have this asset without an asset id, it's just a runtime concept
            // in the future, we won't be "loading from gltf". We'll "import" gltf into mindseye assets, and load those, so this concept of a meAsset with no MAID will go away
            entity.mesh = meAsset(meshHandle, MAMesh);
			entity.authoritativeBounds = mesh.meshBounds; // may change due to anims. Default initialized to mesh bounds
		}
		DynArrayPush(entities, entityAsset);
	}
}
