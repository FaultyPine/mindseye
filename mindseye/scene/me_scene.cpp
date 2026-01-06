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

void meSceneInitialize(EngineContext* ctx)
{
	ctx->sceneSystem = MENEW(&ctx->engineArena, meSceneManager);
}

void meSceneManager::Tick(EngineContext* ctx)
{
	rootScene.mainCamera.UpdateCameraWithUserInput(*ctx->osData);
}

void meSceneManager::LoadSceneFromFileBlocking(StringView filename, meAllocator* sceneAllocator, meScene* outScene)
{
	if (!outScene->runtime.entities)
	{
		outScene->runtime.entities = DynArrayCreate<EntityRef>(sceneAllocator);
	} 
	StringView assetPath = meAssetResource(filename);
    bool success = false;
    {
        OSFileReference file;
        meOSOpenFile(file, assetPath, OSFileFlags(OnlyIfExists | ScopedFile));
        ScopedAllocation tempFileContent(GetTLScratch(), meOSGetFileSize(file));
        meOSReadFileContents(file, tempFileContent.allocation, tempFileContent.allocation.size);
        success = DeserializeFromTextBlocking(TD_MESCENE, sceneAllocator, StringView(tempFileContent.allocation), SPAN_FROM(*outScene));
    }
	if (success)
	{
		if (FindInString(outScene->externalScenePath, STRING_LIT(".gltf")) != -1 ||
			FindInString(outScene->externalScenePath, STRING_LIT(".glb")) != -1)
		{
			meSceneLoadFromGLTF(sceneAllocator, outScene->externalScenePath, *outScene);
		}
	}
    outScene->sceneAssetPath = filename;
}

void meSceneManager::WriteSceneToFileBlocking(meScene* scene, StringView filename)
{
    meAllocator* tempAllocator = GetTLScratch();
    StringView sceneString = {};
	meSerializeResult res = SerializeToTextBlocking(TD_MESCENE, scene, tempAllocator, sceneString);
    ME_ASSERT(res == SER_SUCCESS);
    OSFileReference file;
    meOSOpenFile(file, filename, OSFileFlags(OnlyIfExists | ScopedFile));
    if (!meOSWriteFileContent(file, sceneString.data, sceneString.len))
    {
        LOG_ERROR("Failed to write scene to file. filename = " STRING_FMT "\nsceneString = " STRING_FMT, STRING_VAARGS(filename), STRING_VAARGS(sceneString));
    }
}

void meSceneManager::UnloadCurrentScene()
{
	cgltf_free(rootScene.runtime.gltfData);
	rootScene.runtime.gltfData = nullptr;
	rootScene = {};

	meAllocator* sceneAllocator = &GetEngineCtx()->engineSceneAllocator;
	sceneAllocator->meClear();
	Entity::ReinitializeEntitySystem();
}

void meSceneManager::ChangeCurrentSceneBlocking(StringView filename)
{
	if (FindInString(filename, STRING_LIT(".scn")) == -1)
	{
		LOG_ERROR("Can't load %.*s as a mindseye scene. Mindseye scene files have a .scn extension", STRING_VAARGS(filename));
		return;
	}
	UnloadCurrentScene();
	LoadSceneFromFileBlocking(filename, &GetEngineCtx()->engineSceneAllocator, &this->CurrentScene());
}

void meSceneManager::CopyToRenderInput(meScene& outScene)
{
	// copy the "current"? scene to the given scene for the renderer to use as its readonly copy. This will become complex later...
	outScene = rootScene;
}

meScene& meSceneManager::CurrentScene()
{
	return GetEngineCtx()->sceneSystem->rootScene;
}

struct meSceneAssetLoader : public meAssetLoader
{
	virtual meRTAsset meAssetLoad(meAssetIdent ident)
	{
        EngineContext* engine = GetEngineCtx();
		meRTAsset result = {};
		result.id = ident.id;
		result.type = meAssetType::MAScene;
		result.loadStage = Loaded;
        // TODO: implement async scene loading, so this would return loadStage=Loading
        // and would itself enqueue more asset compiling jobs for the individual parts of the scene
        meScene* resultingScene = MENEW(&engine->engineSceneAllocator, meScene);
		engine->sceneSystem->LoadSceneFromFileBlocking(ident.diskIdent, &engine->engineSceneAllocator, resultingScene);
        result.loadedData = meOwningSpan(resultingScene, sizeof(meScene));
		return result;
	}

	static void RegisterAssetLoader(meEventPayload payload)
	{
		meAllocator* allocator = (meAllocator*)payload.payload;
		meAssetRegisterLoader(MENEW(allocator, meSceneAssetLoader), meAssetType::MAScene);
	}
};

MEEVENT_REGISTER_STATIC(registerAssetLoader, meSceneAssetLoader::RegisterAssetLoader);

void LoadSceneRuntimeFromGLTF(meScene& outScene, meAllocator* sceneAllocator)
{
	if (outScene.IsValid())
	{
		const cgltf_scene& scene = *outScene.runtime.gltfData->scene;
		StringView gltfResPath = outScene.runtime.gltfResourcePath;
		meMeshPool& meshPool = meMeshPoolGet();
		if (!outScene.runtime.entities) outScene.runtime.entities = DynArrayCreate<EntityRef>(sceneAllocator);
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
				entity.mesh = meshHandle;
				entity.authoritativeBounds = mesh.meshBounds; // may change due to anims. Default initialized to mesh bounds
			}
			DynArrayPush(outScene.runtime.entities, entityRef);
		}
	}
}

void meSceneLoadFromGLTF(
	meAllocator* sceneAllocator, 
	StringView resourcePathSv, 
	meScene& outScene)
{
    StringView resourcePath = meAssetResource(resourcePathSv);
	OSFileReference file;
    meOSOpenFile(file, resourcePath, (OSFileFlags)(OSFileFlags::OnlyIfExists | OSFileFlags::ScopedFile));
	u64 filesize = meOSGetFileSize(file);
	Allocation gltfBuffer = MEALLOC(sceneAllocator, filesize);
    if (!meOSReadFileContents(file, gltfBuffer.data, gltfBuffer.size))
    {
        LOG_ERROR("[meScene] failed to load gltf scene %.*s", STRING_VAARGS(resourcePath));
    }
    cgltf_options options = {};
    cgltf_data* data = nullptr;
    ME_ON_SCOPE_EXIT([data]()
    {
        cgltf_free(data);
    });
	// parses the gltf json metadata
    cgltf_result parseResult = cgltf_parse(&options, gltfBuffer.data, gltfBuffer.size, &data);
    if (parseResult == cgltf_result_success)
    {
		// loads the external buffers (actual geo, textures, etc)
        parseResult = cgltf_load_buffers(&options, data, resourcePath.cstr());
		if (parseResult != cgltf_result_success)
		{
			LOG_WARN("Failed to load gltf buffers from %.*s", STRING_VAARGS(resourcePath));
		}
    }
	else
	{
		LOG_WARN("Failed to parse gltf from %.*s", STRING_VAARGS(resourcePath));
	}

	StringView gltfResourcePath = msFsGetDirFromPath(resourcePath);
	outScene.runtime.gltfResourcePath = String(gltfResourcePath, sceneAllocator);
	outScene.runtime.gltfData = data;
	LoadSceneRuntimeFromGLTF(outScene, sceneAllocator);
}