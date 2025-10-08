#pragma once

#include "core/me_defines.h"
// this'll need to know about renderables, transform hierarchies, some settings
#include "core/containers/me_span.h"
#include "generatedtypes/me_scene.generated.h"

typedef u32 meSceneID;

struct cgltf_data;
struct SceneRuntimeData
{
	String gltfResourcePath = {};
	cgltf_data* gltfData = nullptr; // todo: don't cache the cgltf, parse it into my own structures
};

struct MEREFLECT(type, Description="Scene Description", Version=0)
meScene
{
	String externalScenePath;

	MEREFLECT(exclude) 
	SceneRuntimeData runtime = {};
};

struct meSceneManager
{
	// ------------- externally callable -----------------------------------------
	MEAPI void LoadSceneFromFileBlocking(StringView filename, meAllocator* allocator, meScene* outScene);
	MEAPI void WriteSceneToFileBlocking(meScene* scene, StringView filename);

	// -------- engine internal --------------------------
	void Tick(EngineContext* ctx);
	void CopyToRenderInput(meScene& outScene);

	meScene scene;
};

void meSceneInitialize(EngineContext* ctx);

// loads gltf scene from the filesystem
MEAPI void meSceneLoadFromGLTF(meAllocator* allocator, StringView resourcePath, meScene& outScene);

