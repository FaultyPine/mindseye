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

struct MEREFLECT(type, Description="Scene Description")
meScene
{
	MEREFLECT(tooltip="a tooltip here!") 
	u32 numRootNodes = 0;
	
	float someotherfield = 1.0f;

	MEREFLECT(exclude) 
	SceneRuntimeData runtime = {};
};

// loads gltf scene from the filesystem into the specified backing buffer
meSceneID meSceneLoadFromGLTF(meSpan backingBuffer, const char* resourcePath);

