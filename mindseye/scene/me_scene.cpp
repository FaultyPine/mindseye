#include "me_scene.h"

#define CGLTF_IMPLEMENTATION
#include "external/cgltf.h"
#include "asset/me_asset.h"
#include "core/me_scope_exit.h"
#include "core/me_log.h"

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

meSceneID meSceneLoadFromGLTF(meSpan gltfBuffer, const char* resourcePath)
{
    meSceneID scene = U32_INVALID_ID;
    resourcePath = meAssetResource(resourcePath);
    OSFileReference file = {.flags = ScopedFile};
    meOSOpenFile(file, resourcePath);
    ME_ASSERT(meOSGetFileSize(file) <= gltfBuffer.size);
    if (!meOSReadFileContents(file, gltfBuffer.data, gltfBuffer.size))
    {
        LOG_ERROR("[meScene] failed to load gltf scene %s", resourcePath);
        return scene;
    }
    cgltf_options options = {};
    cgltf_data* data = NULL;
    ON_SCOPE_EXIT([data]()
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