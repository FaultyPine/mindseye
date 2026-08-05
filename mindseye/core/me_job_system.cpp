#include "me_job_system.h"

#include "core/me_log.h"
#include "core/thread/me_thread.h"

meJobSystem::~meJobSystem()
{
	Shutdown();
}

void meJobSystem::Initialize(meAllocator* allocator, u32 numThreads)
{
	ME_ASSERT(meOSAtomicLoad(isRunning) == 0);
	ME_ASSERT(numThreads <= MAX_JOB_WORKERS);
	this->numThreads = numThreads;
	this->allocator = allocator;
	meOSAtomicStore(isRunning, 1);
	
	LOG_INFO("[meJobSystem] Spinning up %u threads", numThreads);
	for (u32 threadID = 0; threadID < numThreads; threadID++) 
	{
        workers[threadID] = std::thread([this, threadID]
		{
			StringView threadName = StringFormatTmp("Job Thread %i", threadID);
			meThreadSetName(threadName.cstr());
            meJob job;
            while (meOSAtomicLoad(isRunning)) 
			{ 
                while (jobPool.try_dequeue(job)) 
				{
                    job.func();
                }
				// the threadlocal scratch mem is meant to be cleared when any thread isn't doing "work"
				// for the main thread, that might be at the end of the frame. For workers, that's when there's no jobs
				GetTLScratch()->meClear();
				// TODO: yield or semaphore or atomic spin or something better
				meThreadSleep(1);
            }

        });
    }
}

void meJobSystem::Shutdown()
{
	if (!meOSAtomicExchange(isRunning, 0))
	{
		return;
	}

	for (u32 threadID = 0; threadID < numThreads; threadID++)
	{
		std::thread& worker = workers[threadID];
		if (worker.joinable())
		{
			worker.join();
		}
	}

	numThreads = 0;
}

meJobId meJobSystem::Execute(std::function<void()> jobCb)
{
	meJob job = { .func = jobCb, .id = meOSAtomicAdd(currentJobID, 1) };
	jobPool.enqueue(job);
	return job.id;
}

void meJobSystem::WaitOnJob(meJobId id)
{
	UNIMPLEMENTED();
}


