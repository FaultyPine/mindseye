#pragma once

#include "core/me_defines.h"
// this'll need to know about renderables, transform hierarchies, some settings
#include "core/containers/me_span.h"
#include "scene/me_entity.h"
#include "render/me_camera.h"
#include "core/containers/dynarray.h"
#include "asset/me_asset.h"
#include "generatedtypes/me_scene.generated.h"

typedef u32 meSceneID;

struct MEREFLECT(type, Description="Scene Description")
meScene
{
	ME_ASSET_STRUCTURE(meScene)

	// an external file that represents the scene - i.e. gltf
	String externalScenePath = {};
	meCamera mainCamera = {};

	DynArray<EntityRef> entities = {};
	EntityRef testEntity = {};
};

struct meSceneManager
{
	MEAPI void ChangeCurrentScene(
		StringView filename);

	// -------- engine internal --------------------------
	void UnloadCurrentScene();
	void Tick(EngineContext* ctx);
	void CopyToRenderInput(meScene& outScene);

	Eye rootScene = {};

	MEAPI meScene& CurrentScene();
};


struct meScenePool : public meResourcePool<meScene>
{
	meScenePool(
		meAllocator* resourceAllocator,
		meAllocator* payloadAllocator) :
		meResourcePool<meScene>(resourceAllocator, payloadAllocator) {}

	meAssetType GetAssetType() const
	{
		return MAScene;
	}

	// loads gltf scene from the filesystem
	void Load(
		meAssetIdent ident,
		meAllocator* allocator, 
		StringView resourcePath, 
		meScene& outScene);
};

meScenePool& meScenePoolGet();

void meSceneInitialize(EngineContext* ctx);

