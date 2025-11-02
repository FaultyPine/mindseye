#pragma once

#include "core/me_core.h"
#include "me_gpu.h"

struct meMesh
{
	String name;
	meGPUBuffer idxBuffer = {};
	meGPUBuffer vertBuffer = {};
	meGPUBuffer normBuffer = {};
	meGPUBuffer texcoordBuffer = {};
	BoundingBox meshBounds = {};

	Eye materialHandle = {};

	bool IsLoaded() const { return vertBuffer.IsValid() && idxBuffer.IsValid(); }
};

struct meMeshPool : public meResourcePool<meMesh>
{
	meMeshPool(
		meAllocator* resourceAllocator,
		meAllocator* payloadAllocator) :
	meResourcePool<meMesh>(resourceAllocator, payloadAllocator) {}
};


void meMeshInitialize(EngineContext* engine);

meMeshPool& meMeshPoolGet();
