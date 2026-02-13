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
	// start with a "blank" new scene
	engine->sceneSystem->rootScene = meAssetCreateNew(MAScene).runtimeHandle;
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
	rootScene = {};

	meAllocator* sceneAllocator = &GetEngineCtx()->engineSceneAllocator;
	sceneAllocator->meClear();
	Entity::ReinitializeEntitySystem();
}

void meSceneManager::ChangeCurrentScene(StringView filename)
{
	UnloadCurrentScene();
	meAssetIdent sceneIdent = meAssetGetIdentFromPath(filename);
	meAssetRequestLoad(&sceneIdent, 1);
}

void meSceneManager::CopyToRenderInput(meScene& outScene)
{
	// copy the "current"? scene to the given scene for the renderer to use as its readonly copy. This will become complex later...
	outScene = CurrentScene();
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
	virtual void meAssetLoad(meAsset& asset) override
	{
        EngineContext* engine = GetEngineCtx();
		ME_ASSERT(asset.ident.id.GetType() == MAScene);
		meAllocator* sceneAllocator = &engine->engineSceneAllocator;
        // TODO: implement async scene loading, so this would return loadStage=Loading
        // and would itself enqueue more asset compiling jobs for the individual parts of the scene
		meScenePool& scenePool = meScenePoolGet();
		asset.runtimeHandle = scenePool.CreateInternal();
		meScene& outScene = scenePool.Get(asset.runtimeHandle);
		if (!outScene.entities)
		{
			outScene.entities = DynArrayCreate<EntityRef>(sceneAllocator);
		}
		StringView assetPath = meAssetGetAbsPathForResource(asset.ident.diskIdent);
		meSerializeResult result = SerializeFromFile(assetPath, sceneAllocator, TD_MESCENE, SPAN_FROM(outScene));
		if (result)
		{
			// TODO: individual loaders need to set this stuff after deserializing, but this should be generic for all loaders
			ME_ASSERT(outScene.header.GetID() == asset.ident.id.GetID());
			outScene.header.SetType(asset.ident.id.GetType()); // this should be automatic for all asset types
			if (FindInString(outScene.externalScenePath, STRING_LIT(".gltf")) != -1 ||
				FindInString(outScene.externalScenePath, STRING_LIT(".glb")) != -1)
			{
				meScenePoolGet().Load(asset.ident, sceneAllocator, outScene.externalScenePath, outScene);
			}
		}
		else
		{
			LOG_WARN("Failed to load scene " STRING_FMT, STRING_VAARGS(asset.ident.diskIdent));
		}
		asset.loadStage = result ? Loaded : Unloaded;
	}

	virtual void meAssetWrite(meAsset& asset) override
	{
		meScenePool& scenePool = meScenePoolGet();
		if (!asset.runtimeHandle || asset.loadStage != Loaded)
		{
			LOG_WARN("Attempted to write an unloaded scene asset");
			return;
		}
		meScene& scene = scenePool.Get(asset.runtimeHandle);
		StringView assetPath = meAssetGetAbsPathForResource(asset.ident.diskIdent);
		meAllocator* tempAllocator = GetTLScratch();
		StringView sceneString = {};
		meSerializeResult res = SerializeToTextBlocking(TD_MESCENE, &scene, tempAllocator, sceneString);
		ME_ASSERT(res == meSerializeResult::SER_SUCCESS);
		OSFileReference file;
		meOSOpenFile(file, assetPath, (OSFileFlags_StompExisting | OSFileFlags_ScopedFile));
		if (!meOSWriteFileContent(file, sceneString.data, sceneString.len))
		{
			LOG_ERROR("Failed to write scene to file. filename = " STRING_FMT "\nsceneString = " STRING_FMT, STRING_VAARGS(assetPath), STRING_VAARGS(sceneString));
		}
	}

	virtual const meTypeDescriptor& meAssetGetTypeDescriptor() override
	{
		return TD_MESCENE;
	}
	virtual meResourcePoolBase* meAssetGetResourcePool() override
	{
		return &meScenePoolGet();
	}
	virtual void meAssetOnLoad(meAsset& asset) override
	{
		GetEngineCtx()->sceneSystem->rootScene = asset.runtimeHandle;
		GetEngineCtx()->appCallbacks.onSceneLoadFn(GetEngineCtx());
	}

	static void RegisterAssetLoader(meEventPayload payload)
	{
		meAllocator* allocator = (meAllocator*)payload.payload;
		meAssetRegisterLoader(MENEW(allocator, meSceneAssetLoader), meAssetType::MAScene);
	}
};

MEEVENT_REGISTER_STATIC(registerAssetLoader, meSceneAssetLoader::RegisterAssetLoader);

void meScenePool::Load(
	meAssetIdent ident,
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
	DynArray<EntityRef>& entities = outScene.entities;
	StringView gltfResPath = msFsGetDirFromPath(resourcePathAbs);
	meMeshPool& meshPool = meMeshPoolGet();
	for (u64 nodeIdx = 0; nodeIdx < scene.nodes_count; nodeIdx++)
	{
		const cgltf_node& node = *scene.nodes[nodeIdx];
		float nodeMatrix[16];
		cgltf_node_transform_local(&node, nodeMatrix);
		meTransform nodeTf = meTransform(glm::make_mat4(nodeMatrix));
		EntityRef entityRef = Entity::CreateEntity(StringFromCString(node.name), nodeTf);
		EntityData& entity = Entity::GetEntity(entityRef);
		if (node.mesh)
		{
			const cgltf_mesh& gltfmesh = *node.mesh;
			meMeshID meshHandle = meshPool.Load(GetEngineCtx()->renderer, gltfResPath, gltfmesh);
			meMesh& mesh = meshPool.Get(meshHandle);
			entity.mesh = meAsset(meshHandle);
			entity.authoritativeBounds = mesh.meshBounds; // may change due to anims. Default initialized to mesh bounds
		}
		DynArrayPush(entities, entityRef);
	}
}
