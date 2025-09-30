#pragma once

#include "me_gpu.h"

struct meMesh
{
	meGPUBuffer idxBuffer;
	meGPUBuffer vertBuffer;
	meGPUBuffer normBuffer;
	meGPUBuffer texcoordBuffer;
	void* vertexLayout;

	bool IsLoaded() const { return vertBuffer.IsValid() && idxBuffer.IsValid(); }
};

