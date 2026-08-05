#pragma once

#include "core/me_defines.h"
#include "core/containers/me_array.h"
#include "platform/me_os.h"
#include "external/concurrentqueue/concurrentqueue.h"
#include <functional> // need lambdas... captures are useful.
#include <thread>

typedef u32 meJobId;
typedef void(*meJobCb)(void* payload);

// FUTURE:
// big plans...
// for reference: https://github.com/dougbinks/enkiTS
// DESIGN:
/*
should be lock free and use work stealing algo
should be able to build & submit a "graph" of work - the dependencies of which are resolved and the work is scheduled in parallel based on that graph
need "job queue types" - basically separate buckets of work that can be synchronized differently. I.E. background async work, asset compilation, frame tasks, multi-frame async tasks, etc.
A job should be able to enqueue more jobs inside itself
a job should be able to enqueue child jobs from itself
Need to be able to wait on a job, or on a full "graph"/batch of submitted jobs
A job should be able to return out as "suspended". Which will re-queue the job, and call it as normal. 
    SUSPEND = scheduler will requeue the job as normal (programmer can add checks to skip logic on next passes if desired), 
    WAITING = job is waiting on a synchronization primitive, sheduler shouldn't requeue it until the thing (?) is signaled (need a way to represent jobs that wait on some signal at the scheduler level)
*/


struct meJob
{
	std::function<void()> func;
	meJobId id;
};

struct meJobSystem
{
	meJobSystem() 
		: numThreads(0), allocator(nullptr) {}
	MEAPI ~meJobSystem();
	MEAPI void Initialize(meAllocator* allocator, u32 numThreads);
	MEAPI void Shutdown();
    MEAPI meJobId Execute(std::function<void()> job);
    MEAPI void WaitOnJob(meJobId id);

	static constexpr u32 MAX_JOB_WORKERS = 32;
	#define MAX_JOBS 256
	u32 numThreads;
	meAtomicU32 isRunning = {};
	// provides unique identifier for every job
	meAtomicU32 currentJobID = {};
	meAllocator* allocator;
	meArray<std::thread, MAX_JOB_WORKERS> workers = {};
	moodycamel::ConcurrentQueue<meJob> jobPool;
};

