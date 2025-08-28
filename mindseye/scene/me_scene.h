#pragma once

#include "core/me_defines.h"
// this'll need to know about renderables, transform hierarchies, some settings
#include "core/containers/me_span.h"

typedef u32 meSceneID;

struct cgltf_data;
struct SceneRuntimeData
{
	cgltf_data* gltfData = nullptr;
};

struct MEREFLECT meScene
{
	u32 numRootNodes = 0;
	MEREFLECTEXCL SceneRuntimeData runtime = {};
};

// loads gltf scene from the filesystem into the specified backing buffer
meSceneID meSceneLoadFromGLTF(meSpan backingBuffer, const char* resourcePath);

