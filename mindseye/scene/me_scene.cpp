#include "me_scene.h"

#define CGLTF_IMPLEMENTATION
#include "external/cgltf.h"
#include "asset/me_asset.h"
#include "core/me_scope_exit.h"
#include "core/me_log.h"
#include "core/me_serialize.h"

#include "generatedtypes/me_scene.generated.cpp"

void meSceneInitialize(EngineContext* ctx)
{
	ctx->sceneSystem = MENEW(&ctx->engineArena, meSceneManager);
}

void meSceneManager::Tick(EngineContext* ctx)
{
	rootScene.mainCamera.UpdateCameraWithUserInput(ctx->osData->mouseState);
}

void meSceneManager::LoadSceneFromFileBlocking(StringView filename, meAllocator* sceneAllocator, meScene* outScene)
{
	StringView assetPath = meAssetResource(filename);
	bool success = DeserializeFromIniBlocking(TD_MESCENE, sceneAllocator, assetPath, meSpan(outScene, sizeof(*outScene)));
	if (success)
	{
		if (FindInString(outScene->externalScenePath, STRING_LIT(".gltf")) != -1 ||
			FindInString(outScene->externalScenePath, STRING_LIT(".glb")) != -1)
		{
			meSceneLoadFromGLTF(sceneAllocator, outScene->externalScenePath, *outScene);
		}
	}
}

void meSceneManager::WriteSceneToFileBlocking(meScene* scene, StringView filename)
{
	SerializeToIniBlocking(TD_MESCENE, scene, filename);
}

void meSceneManager::UnloadCurrentScene()
{
	cgltf_free(rootScene.runtime.gltfData);
	rootScene.runtime.gltfData = nullptr;
	rootScene = {};
}

void meSceneManager::ChangeCurrentSceneBlocking(StringView filename)
{
	if (FindInString(filename, STRING_LIT(".scn")) == -1)
	{
		LOG_ERROR("Can't load %.*s as a mindseye scene. Mindseye scene files have a .scn extension", STRING_VAARGS(filename));
		return;
	}
	UnloadCurrentScene();
	meAllocator* sceneAllocator = &GetEngineCtx()->engineSceneAllocator;
	sceneAllocator->meClear();
	LoadSceneFromFileBlocking(filename, sceneAllocator, &this->rootScene);
}

void meSceneManager::CopyToRenderInput(meScene& outScene)
{
	// copy the "current"? scene to the given scene for the renderer to use as its readonly copy. This will become complex later...
	outScene = rootScene;
}

struct meSceneAssetLoader : public meAssetLoader
{
	virtual meRTAsset meAssetLoad(meAssetIdent ident)
	{
		meRTAsset result = {};
		result.id = ident.id;
		result.type = meAssetType::Scene;
		result.loadStage = Loaded;
		// actual loading, need to fill out the loadedData span

		return result;
	}

	static void RegisterAssetLoader(meEventPayload payload)
	{
		meAllocator* allocator = (meAllocator*)payload.payload;
		meAssetRegisterLoader(MENEW(allocator, meSceneAssetLoader), meAssetType::Scene);
	}
};

MEEVENT_REGISTER_STATIC(registerAssetLoader, meSceneAssetLoader::RegisterAssetLoader);

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
	GetEngineCtx()->renderer->LoadSceneRuntime(outScene, sceneAllocator);
}