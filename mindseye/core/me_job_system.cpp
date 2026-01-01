#include "me_job_system.h"

#include "core/me_log.h"
#include "core/thread/me_thread.h"
#include <thread>

void meJobSystem::Initialize(meAllocator* allocator, u32 numThreads)
{
	this->numThreads = numThreads;
	this->allocator = allocator;
	
	LOG_INFO("[meJobSystem] Spinning up %u threads", numThreads);
	for (u32 threadID = 0; threadID < numThreads; threadID++) 
	{
        std::thread worker([this, threadID]
		{
			StringView threadName = StringFormat("Job Thread %i", threadID);
			meThreadSetName(threadName.cstr());
            meJob job;
			// allows us to shut down all threads when program exits by just setting numthreads to 0
            while (this->numThreads > 0) 
			{ 
                if (jobPool.try_dequeue(job)) 
				{
                    job.func();
                }
				// put thread to sleep here
            }

        });
        worker.detach();
    }
}

meJobId meJobSystem::Execute(std::function<void()> jobCb)
{
	meJob job = { .func = jobCb, .id = currentJobID++ };
	jobPool.enqueue(job);
	return job.id;
}

void meJobSystem::WaitOnJob(meJobId id)
{
	UNIMPLEMENTED();
}


