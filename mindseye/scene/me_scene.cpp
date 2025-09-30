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
	
}

void meSceneManager::LoadSceneFromFileBlocking(StringView filename, meAllocator* allocator, meScene* outScene)
{
	meSpan deserializedScene = DeserializeFromIniBlocking(TD_MESCENE, allocator, filename);
	if (deserializedScene)
	{
		*outScene = *(meScene*)deserializedScene.data;
	}
}

void meSceneManager::WriteSceneToFileBlocking(meScene* scene, StringView filename)
{
	SerializeToIniBlocking(TD_MESCENE, scene, filename);
}

void meSceneManager::CopyToRenderInput(meScene& outScene)
{
	// copy the "current"? scene to the given scene for the renderer to use as its readonly copy. This will become complex later...
	outScene = scene;
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
	meAllocator* allocator, 
	StringView resourcePathSv, 
	meScene& outScene)
{
    const char* resourcePath = meAssetResource(resourcePathSv);
    OSFileReference file = {.flags = ScopedFile};
    meOSOpenFile(file, StringView(resourcePath, CStringLength(resourcePath)), OSFileFlags::OnlyIfExists);
	u64 filesize = meOSGetFileSize(file);
	Allocation gltfBuffer = MEALLOC(allocator, filesize);
    if (!meOSReadFileContents(file, gltfBuffer.data, gltfBuffer.size))
    {
        LOG_ERROR("[meScene] failed to load gltf scene %s", resourcePath);
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
        parseResult = cgltf_load_buffers(&options, data, resourcePath);
		if (parseResult != cgltf_result_success)
		{
			LOG_WARN("Failed to load gltf buffers from %s", resourcePath);
		}
    }
	else
	{
		LOG_WARN("Failed to parse gltf from %s", resourcePath);
	}
	// tmp
	const char* sceneName = data->scene->name;
	if (!sceneName)
	{
		sceneName = data->nodes_count ? data->nodes[0].name : "Unnamed scene";
	}
	outScene.sceneName = String(sceneName, CStringLength(sceneName), allocator);
	outScene.runtime.gltfData = data;
}