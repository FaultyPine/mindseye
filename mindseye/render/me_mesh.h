#pragma once

#include "core/me_core.h"
#include "me_gpu.h"

struct meMesh
{
	meGPUBuffer idxBuffer;
	meGPUBuffer vertBuffer;
	meGPUBuffer normBuffer;
	meGPUBuffer texcoordBuffer;
	void* vertexLayout;

	Eye materialHandle;

	bool IsLoaded() const { return vertBuffer.IsValid() && idxBuffer.IsValid(); }
};

