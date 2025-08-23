#pragma once

// this'll need to know about renderables, transform hierarchies, some settings
#include "core/containers/me_span.h"

struct meScene
{
	u32 numRootNodes;
};
typedef u32 meSceneID;

// loads gltf scene from the filesystem into the specified backing buffer
meSceneID meSceneLoadFromGLTF(meSpan backingBuffer, const char* resourcePath);

