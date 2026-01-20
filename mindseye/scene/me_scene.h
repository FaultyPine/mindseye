#pragma once

#include "core/me_defines.h"
// this'll need to know about renderables, transform hierarchies, some settings
#include "core/containers/me_span.h"
#include "scene/me_entity.h"
#include "render/me_camera.h"
#include "core/containers/dynarray.h"
#include "generatedtypes/me_scene.generated.h"

typedef u32 meSceneID;

struct MEREFLECT(type, Description="Scene Description", Version=0)
meScene
{
	// meScene stores data about the structure of the scene
	String externalScenePath = {};
	meCamera mainCamera = {};

	String sceneAssetPath = {};

	DynArray<EntityRef> entities = {};
	EntityRef testEntity = {};
};

struct meSceneManager
{
	// ------------- externally callable -----------------------------------------
	MEAPI void WriteSceneToFileBlocking(
		meScene* scene, 
		StringView filename);
	MEAPI void ChangeCurrentSceneBlocking(
		StringView filename);

	// -------- engine internal --------------------------
	void UnloadCurrentScene();
	void Tick(EngineContext* ctx);
	void CopyToRenderInput(meScene& outScene);

	// TODO: store this somewhere better?
	// maybe scenes should use a resourcepool too?
	meScene rootScene = {};

	static MEAPI meScene& CurrentScene();
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

