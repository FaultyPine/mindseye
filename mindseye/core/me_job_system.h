#pragma once

#include "core/me_defines.h"
#include "external/concurrentqueue/concurrentqueue.h"
#include <functional> // need lambdas... captures are useful.

typedef u32 meJobId;
typedef void(*meJobCb)(void* payload);

// FUTURE:
// replace with lock free work stealing algo
// spin on atomics w/cpu yields
// or 3rd party lib equivalent (https://gametechdev.github.io/GTS-GamesTaskScheduler/documentation/html/index.html)
// big plans...
// need Queue Types (background async, asset compilation, frame tasks, multi-frame async tasks, ...)


struct meJob
{
	std::function<void()> func;
	meJobId id;
};

struct meJobSystem
{
	meJobSystem() 
		: numThreads(1), currentJobID(0), allocator(nullptr) {}
	MEAPI void Initialize(meAllocator* allocator, u32 numThreads);
    MEAPI void Shutdown() { numThreads = 0; }
    MEAPI meJobId Execute(std::function<void()> job);
    MEAPI void WaitOnJob(meJobId id);

	#define MAX_JOBS 256
	u32 numThreads;
	// provides unique identifier for every job
	meJobId currentJobID;
	meAllocator* allocator;
	moodycamel::ConcurrentQueue<meJob> jobPool;
};

