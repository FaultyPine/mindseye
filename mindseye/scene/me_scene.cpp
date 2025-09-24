#include "me_scene.h"

#define CGLTF_IMPLEMENTATION
#include "external/cgltf.h"
#include "asset/me_asset.h"
#include "core/me_scope_exit.h"
#include "core/me_log.h"
#include "core/me_serialize.h"

#include "generatedtypes/me_scene.generated.cpp"

void meSceneManager::Tick(EngineContext* ctx)
{
	
}

meSceneManager::meSceneManager(EngineContext* ctx)
{
	StringView filename = STRING_LIT("TestSceneSerialized.scn");
	meScene scene = { .numRootNodes = 2, .someotherfield = 0.3 };
	WriteSceneToFileBlocking(&scene, filename);
	scene = {};
	LoadSceneFromFileBlocking(filename, &ctx->engineSceneAllocator, &scene);
	LOG_INFO("deserialized scene %u %f", scene.numRootNodes, scene.someotherfield);
}

void meSceneManager::LoadSceneFromFileBlocking(StringView filename, meAllocator* allocator, meScene* outScene)
{
	meSpan deserializedScene = DeserializeFromIniBlocking(g_meScene_typedescriptor, allocator, filename);
	if (deserializedScene)
	{
		*outScene = *(meScene*)deserializedScene.data;
	}
}

void meSceneManager::WriteSceneToFileBlocking(meScene* scene, StringView filename)
{
	SerializeToIniBlocking(g_meScene_typedescriptor, scene, &GetEngineCtx()->scratchWork, filename);
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

meSceneID meSceneLoadFromGLTF(meSpan gltfBuffer, StringView resourcePathSv)
{
    meSceneID scene = U32_INVALID_ID;
    const char* resourcePath = meAssetResource(resourcePathSv);
    OSFileReference file = {.flags = ScopedFile};
    meOSOpenFile(file, StringView(resourcePath, CStringLength(resourcePath)));
    ME_ASSERT(meOSGetFileSize(file) <= gltfBuffer.size);
    if (!meOSReadFileContents(file, gltfBuffer.data, gltfBuffer.size))
    {
        LOG_ERROR("[meScene] failed to load gltf scene %s", resourcePath);
        return scene;
    }
    cgltf_options options = {};
    cgltf_data* data = nullptr;
    ME_ON_SCOPE_EXIT([data]()
    {
        cgltf_free(data);
    });
    cgltf_result parseResult = cgltf_parse(&options, gltfBuffer.data, gltfBuffer.size, &data);
    if (parseResult == cgltf_result_success)
    {
        cgltf_load_buffers(&options, data, resourcePath);
    }
    return scene;
}