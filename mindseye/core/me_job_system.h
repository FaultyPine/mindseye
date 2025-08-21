#pragma once

#include "core/me_defines.h"
#include "external/concurrentqueue/concurrentqueue.h"

typedef u32 meJobId;
typedef void(*meJobCb)(void* payload);

struct meJob
{
	meJobCb func;
	meJobId id;
	void* payload;
};

struct meJobSystem
{
	meJobSystem() 
		: numThreads(1), currentJobID(0), allocator(nullptr) {}
	MEAPI void Initialize(meAllocator* allocator, u32 numThreads);
    MEAPI void Shutdown() { numThreads = 0; }
    MEAPI meJobId Execute(meJobCb job, void* payload);
    MEAPI void WaitOnJob(meJobId id);

	#define MAX_JOBS 256
	u32 numThreads;
	// provides unique identifier for every job
	meJobId currentJobID;
	meAllocator* allocator;
	moodycamel::ConcurrentQueue<meJob> jobPool;
};

