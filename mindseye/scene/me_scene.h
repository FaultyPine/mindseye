#pragma once

#include "core/me_defines.h"
// this'll need to know about renderables, transform hierarchies, some settings
#include "core/containers/me_span.h"
#include "generatedtypes/me_scene.generated.h"

typedef u32 meSceneID;

struct cgltf_data;
struct SceneRuntimeData
{
	cgltf_data* gltfData = nullptr; // todo: don't cache the cgltf, parse it into my own structures
};

struct MEREFLECT(type, Description="Scene Description", Version=0)
meScene
{
	MAID sceneAssetID;

	MEREFLECT(exclude) 
	SceneRuntimeData runtime = {};
};

struct meSceneManager
{
	void Tick(EngineContext* ctx);
	void LoadSceneFromFileBlocking(StringView filename, meAllocator* allocator, meScene* outScene);
	void WriteSceneToFileBlocking(meScene* scene, StringView filename);
};

void meSceneInitialize(EngineContext* ctx);

// loads gltf scene from the filesystem into the specified backing buffer
meSceneID meSceneLoadFromGLTF(meSpan backingBuffer, StringView resourcePath);

