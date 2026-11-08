#include "core/me_app.h"
#include "core/me_profile.h"
#include "me_scene.h"

#include "core/me_memory.h"
#define CGLTF_MALLOC(size) GetDefaultAllocator()->meAlloc(size).data
#define CGLTF_FREE(ptr) GetDefaultAllocator()->meFree(ptr)
#define CGLTF_IMPLEMENTATION
#include "external/cgltf.h"
#undef CGLTF_IMPLEMENTATION
#undef CGLTF_MALLOC
#undef CGLTF_FREE
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

    virtual bool meAssetImport(OSFileReference importThis) override
    {
        ME_PROFILE_FUNCTION();
        StringView sceneFilePath = importThis.GetPath();
		if (FindInString(sceneFilePath, STRING_LIT(".gltf")) != -1 ||
			FindInString(sceneFilePath, STRING_LIT(".glb")) != -1)
		{
            meAsset newAsset{};
			if (meAssetImportGltfScene(newAsset, importThis))
            {
                return meAssetWrite(newAsset);
            }
		}
        return false;
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

