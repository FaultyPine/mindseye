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
        std::thread worker([this, threadID, &numThreads]
		{
			meThreadSetName(TextFormat("Job Thread %i", threadID));
            meJob job;
			// allows us to shut down all threads when program exits by just setting numthreads to 0
            while (numThreads > 0) 
			{ 
                if (jobPool.try_dequeue(job)) 
				{
                    job.func(job.payload);
                }
				// put thread to sleep here
            }

        });
        worker.detach();
    }
}

meJobId meJobSystem::Execute(meJobCb jobCb, void* payload)
{
	meJob job = { .func = jobCb, .id = currentJobID++, .payload = payload };
	jobPool.enqueue(job);
	return job.id;
}

void meJobSystem::WaitOnJob(meJobId id)
{
	UNIMPLEMENTED();
}


