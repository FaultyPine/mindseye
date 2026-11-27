#pragma once

#include "core/me_defines.h"
#include "core/me_ptrs.h"
#include "core/me_core.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-attributes"
#include <taskflow/taskflow.hpp>
#pragma clang diagnostic pop

// data that should persist for the lifetime of the task graph lives in the root task's callable (usually lambda capture)
struct meTaskGraph : public tf::Taskflow
{
    tf::Future<void> complete;

    template <typename... Args>
    static meTaskGraph Create(Args&&... args)
    {
        return { .complete = {}, };
    }

    bool IsComplete() const
    {
        return complete.valid() && complete.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    void Wait(u64 timeoutMicroseconds = UINT64_MAX)
    {
        complete.wait_for(std::chrono::microseconds(timeoutMicroseconds));
    }
};

struct meTaskManager : public tf::Executor
{
    using tf::Executor::Executor;

    void Drain()
    {
        wait_for_all();
    }

    // takes ownership of the passed in graph
    void Submit(meTaskGraph& graph)
    {
        Submit(graph, []{});
    }
    
    // takes ownership of the passed in graph
    template <typename OnCompleteCallable>
    void Submit(meTaskGraph& graph, OnCompleteCallable& complete)
    {
        if (graph.complete.valid())
        {
            ME_ASSERT(false && "Task graphs can only be submitted once");
            return;
        }
        // Transfer the task storage to the executor so the request can outlive its handle.
        graph.complete = run(meMove(graph), meMove(complete));
    }
};
