#pragma once

#include "core/me_defines.h"
// this'll need to know about renderables, transform hierarchies, some settings
#include "core/containers/me_span.h"
#include "scene/me_entity.h"
#include "render/me_camera.h"
#include "core/containers/dynarray.h"
#include "generatedtypes/me_scene.generated.h"

typedef u32 meSceneID;

struct cgltf_data;
struct SceneRuntimeData
{
	String gltfResourcePath = {};
	cgltf_data* gltfData = nullptr; // todo: don't cache the cgltf, parse it into my own structures
	DynArray<EntityRef> entities = {};
};

struct MEREFLECT(type, Description="Scene Description", Version=0)
meScene
{
	// meScene stores data about the structure of the scene
	String externalScenePath = {};
	meCamera mainCamera = {};

	// this stores the scene data necessary at runtime
	MEREFLECT(exclude) 
	SceneRuntimeData runtime = {};

	DynArray<EntityRef> entities = {};
	DynArray<String> entityNamesOrSomething = {};

	MEREFLECT(exclude) 
    String sceneAssetPath = {};

	bool IsValid() const { return runtime.gltfData != nullptr; }
};

struct meSceneManager
{
	// ------------- externally callable -----------------------------------------
	MEAPI void LoadSceneFromFileBlocking(StringView filename, meAllocator* allocator, meScene* outScene);
	MEAPI void WriteSceneToFileBlocking(meScene* scene, StringView filename);
	MEAPI void ChangeCurrentSceneBlocking(StringView filename);

	// -------- engine internal --------------------------
	void UnloadCurrentScene();
	void Tick(EngineContext* ctx);
	void CopyToRenderInput(meScene& outScene);

	// TODO: store this somewhere better?
	// maybe scenes should use a resourcepool too?
	meScene rootScene = {};

	static MEAPI meScene& CurrentScene();
};

void meSceneInitialize(EngineContext* ctx);

// loads gltf scene from the filesystem
MEAPI void meSceneLoadFromGLTF(meAllocator* allocator, StringView resourcePath, meScene& outScene);

