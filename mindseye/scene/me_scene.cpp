#include "core/me_app.h"
#include "core/me_profile.h"
#include "me_scene.h"

#define CGLTF_IMPLEMENTATION
#include "external/cgltf.h"
#include "asset/me_asset.h"
#include "asset/me_asset_import_gltf.h"
#include "asset/me_asset_index.h"
#include "core/me_scope_exit.h"
#include "core/me_log.h"
#include "core/me_serialize.h"
#include "scene/me_entity.h"
#include "render/me_mesh.h"
#include "platform/me_os.h"

ME_DEFINE_LOG_CATEGORY(Scene);

void meSceneInitialize(EngineContext* engine)
{
	engine->sceneSystem = MENEW(&engine->engineArena, meSceneManager);
	engine->scenePool = MENEW(&engine->engineArena, meScenePool, &engine->engineArena, &engine->engineArena);
}

void meSceneInitializeLate(EngineContext* engine)
{
	// start with a "blank" new scene
	engine->sceneSystem->rootScene = meAssetCreateNewAsset(MAScene, meResourceType_InstanceAsset);
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
	ME_PROFILE_FUNCTION();
    EngineContext* ctx = GetEngineCtx();

    if (rootScene.isLoaded())
    {
        meAssetUnloadBlocking(SPAN_FROM_TYPED_SINGLE(rootScene));
        rootScene = {};
    }

    meAllocator* sceneAllocator = &ctx->engineSceneAllocator;
	sceneAllocator->meClear();
}

void meSceneManager::ChangeCurrentSceneAsync(StringView filename)
{
	ME_PROFILE_FUNCTION();
	MAID sceneIdent = meAssetIndexGetMAIDFromPath(filename);
    if (!sceneIdent || sceneIdent.GetType() != MAScene)
    {
        LOG_WARN(Scene, "Failed to change scene. Scene file {} doesn't map to a valid scene asset", LogSV(filename));
        return;
    }
	UnloadCurrentScene();
	meAssetRequestLoadTemplate(&sceneIdent, 1, [](const meAsset& sceneAsset)
    {
        ME_PROFILE_FUNCTION();
        if (meAsset* asset = meAssetTryGetTemplate(sceneAsset.id))
        {
            GetEngineCtx()->sceneSystem->rootScene = *asset;
            meEventPayload payload = {asset};
            GetEngineCtx()->userApp.ActiveCallbacks().onSceneLoaded(payload);
        }    
    });
}

void meSceneManager::CopyToRenderInput(meScene& outScene, meAllocator* frameAllocator)
{
	// copy the "current"? scene to the given scene for the renderer to use as its readonly copy. This will become complex later...
    ME_ASSERT(frameAllocator);
    DeepCopyContext copyCtx = {};
    copyCtx.srcData = &CurrentScene();
    copyCtx.outputData = meSpan(&outScene, sizeof(outScene));
    copyCtx.allocator = frameAllocator;
    meTypeDescriptorDeepCopy(TD_MESCENE, copyCtx);
}

// TODO: BVH
meSceneRaycastHit meSceneRaycast(meScene& scene, const meRay& ray)
{
    ME_PROFILE_FUNCTION();
	meSceneRaycastHit result = {};
	f32 closestT = FLT_MAX;

	DynArray<meTypedAsset<MAEntity>>& entities = scene.entities;
	for (DynArray_Foreach(entities, i))
	{
		meTypedAsset<MAEntity>& entRef = entities[i];
		ScopedAssetLockR<meEntity> entData(entRef);
		if (!entData || meEntityIsFlag(*entData, EntityFlags_HIDDEN) || meEntityIsFlag(*entData, EntityFlags_DISABLED))
			continue;

        ScopedAssetLockR<meMesh> mesh(entData->mesh);
        const BoundingBox& bounds = mesh->meshBounds;
        glm::mat4 toWorld = entData->transform.ToWorldFromModelMatrix();
		glm::vec3 worldMin = entData->transform.position + glm::vec3(toWorld * glm::vec4(bounds.min, 1.0));
		glm::vec3 worldMax = entData->transform.position + glm::vec3(toWorld * glm::vec4(bounds.max, 1.0));

		f32 t = 0.0f;
		if (meRayIntersectsAABB(ray, worldMin, worldMax, t))
		{
			if (t < closestT)
			{
				closestT = t;
				result.entity = entities[i];
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

	virtual bool meAssetPrepareCompiledData(meAsset& asset) override
	{
		ME_PROFILE_FUNCTION();
		meAllocator* allocator = resourcePool->GetPayloadAllocator();
        meScene* outScene = (meScene*)asset.runtimeContent.get();
		if (FindInString(outScene->externalScenePath, STRING_LIT(".gltf")) != -1 ||
			FindInString(outScene->externalScenePath, STRING_LIT(".glb")) != -1)
		{
			return meScenePoolGet().Load(allocator, outScene->externalScenePath, *outScene);
		}
        return true;
	}

    bool meAssetInitializeRuntime(meAsset& asset) override
    {
        meScene& scene = *(meScene*)asset.runtimeContent.get();
        for (DynArray_Foreach(scene.entities, i))
        {
            Eye entityHandle = scene.entities[i].runtimeHandle;
            if (!entityHandle)
            {
                continue; // MAID references are loaded through the asset system.
            }
            meEntity& entity = meEntityGetPool().Get(entityHandle);
            if (entity.mesh.runtimeHandle)
            {
                entity.mesh.runtimeContent = meMeshPoolGet().GetShared(entity.mesh.runtimeHandle);
            }
            if (entity.mesh.runtimeHandle && !meAssetSystemGet().assetLoaders[MAMesh]->meAssetInitializeRuntime(entity.mesh))
            {
                return false;
            }
        }
        return true;
    }

	static void RegisterAssetLoader(meEventPayload payload)
	{
		meAllocator* allocator = (meAllocator*)payload.payload;
		meAssetRegisterLoader(MENEW(allocator, meSceneAssetLoader, &TD_MESCENE, &meScenePoolGet(), meAssetType::MAScene));
	}
};

MEEVENT_REGISTER_STATIC(registerAssetLoader, meSceneAssetLoader::RegisterAssetLoader);

bool meScenePool::Load(
	meAllocator* sceneAllocator,
	StringView resourcePathRel,
	meScene& outScene)
{
	ME_PROFILE_FUNCTION();
	String resourcePathAbs = meAssetGetAbsPathForResource(resourcePathRel);
	meMemoryMappedFile mapping;
    if (!meOSMapFile(mapping, resourcePathAbs, (OSFileFlags_OnlyIfExists | OSFileFlags_ScopedFile | OSFileFlags_ReadOnly)))
    {
        return false;
    }
	meSpan gltfBuffer(mapping.ptr, mapping.size);

    cgltf_options options = {};
    cgltf_data* gltfData = nullptr;

    ME_ON_SCOPE_EXIT([&gltfData]()
	{
		cgltf_free(gltfData);
	});
	// parses the gltf json metadata
    cgltf_result parseResult = {};
	{
		ME_PROFILE_SCOPE("cgltf_parse scene");
		parseResult = cgltf_parse(&options, gltfBuffer.data, gltfBuffer.size, &gltfData);
	}
    if (parseResult == cgltf_result_success)
    {
		// loads the external buffers (actual geo, textures, etc)
		{
			ME_PROFILE_SCOPE("cgltf_load_buffers scene");
			parseResult = cgltf_load_buffers(&options, gltfData, resourcePathAbs.cstr());
		}
		if (parseResult != cgltf_result_success)
		{
			LOG_WARN(Scene, "Failed to load gltf buffers from {}", LogSV(resourcePathAbs));
            return false;
		}
    }
	else
	{
		LOG_WARN(Scene, "Failed to parse gltf from {}", LogSV(resourcePathAbs));
        return false;
	}
	if (!gltfData->scene)
    {
        return false;
    }
	const cgltf_scene& scene = *gltfData->scene;
	DynArray<meTypedAsset<MAEntity>>& entities = outScene.entities;
	StringView gltfResPath = msFsGetDirFromPath(resourcePathAbs);
	meMeshPool& meshPool = meMeshPoolGet();
	for (u64 nodeIdx = 0; nodeIdx < scene.nodes_count; nodeIdx++)
	{
		const cgltf_node& node = *scene.nodes[nodeIdx];
		float nodeMatrix[16];
		cgltf_node_transform_local(&node, nodeMatrix);
		meTransform nodeTf = meTransform(glm::make_mat4(nodeMatrix));
		meTypedAsset<MAEntity> entityAsset = meEntityCreateBlankInstance(StringFromCString(node.name));
		meEntity& entity = meEntityGet(entityAsset);
		entity.transform = nodeTf;
		if (node.mesh)
		{
			const cgltf_mesh& gltfmesh = *node.mesh;
			meMeshID meshHandle = meshPool.Load({.resourceType = meResourceType_InstanceAsset});
            meAsset meshAsset(meshHandle, MAMesh);
            meshAsset.runtimeContent = meshPool.GetShared(meshHandle);
            if (!meAssetImportGltfMesh(meshPool.Get(meshHandle), gltfResPath, gltfmesh))
            {
                meshPool.Destroy(meshHandle);
                return false;
            }
			meMesh& mesh = meshPool.Get(meshHandle);
            // TODO: right now, we have this asset without an asset id, it's just a runtime concept
            // in the future, we won't be "loading from gltf". We'll "import" gltf into mindseye assets, and load those, so this concept of a meAsset with no MAID will go away
            entity.mesh = meshAsset;
			entity.authoritativeBoundsOS = mesh.meshBounds; // may change due to anims. Default initialized to mesh bounds
		}
		DynArrayPush(entities, entityAsset);
	}
    return true;
}
