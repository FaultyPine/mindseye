#include "me_memory.h"
#include "core/containers/me_stack.h"

static Stack<meAllocator, 10> g_allocators;


const meAllocator& AllocatorGet()
{
    return g_allocators.top();
}

static Allocation SystemAlloc(
    u64 size)
{
    return Allocation((u8*)SYSTEM_MALLOC(size), size);
}

static void SystemFree(
    const Allocation& mem)
{
    SYSTEM_FREE(mem.data);
}

static Allocation SystemRealloc(
    const Allocation& mem, 
    u64 newSize)
{
    return Allocation((u8*)SYSTEM_REALLOC(mem.data, newSize), newSize);
}

static void SystemClear()
{
    UNIMPLEMENTED();
}


void InitializeAllocatorSystem()
{
    meAllocator systemAllocator;
    systemAllocator.alloc = SystemAlloc;
    systemAllocator.free = SystemFree;
    systemAllocator.realloc = SystemRealloc;
    systemAllocator.clear = SystemClear;
    AllocatorPush(systemAllocator);
}

void AllocatorPush(const meAllocator& allocator)
{
    g_allocators.push(allocator);
}

meAllocator AllocatorPop()
{
    ME_ASSERT(g_allocators.size() > 1); // can't pop system allocator, which is always the bottom of the stack
    return g_allocators.pop();
}


