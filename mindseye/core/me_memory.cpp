#include "me_memory.h"
#include "core/containers/me_stack.h"

static Stack<meAllocator, 10> g_allocators;

EXT_IMPORT C_LINKAGE void*  malloc (size_t _Size);
EXT_IMPORT C_LINKAGE void   free   (void *_Block);
EXT_IMPORT C_LINKAGE void*  realloc(void *_Block, size_t newSize);
#define SYSTEM_MALLOC(size) malloc(size)
#define SYSTEM_FREE(ptr) free(ptr)
#define SYSTEM_REALLOC(ptr, newSize) realloc(ptr, newSize)

meAllocator& AllocatorGet()
{
    return g_allocators.top();
}

static Allocation SystemAlloc(
    u64 size)
{
    return Allocation((u8*)SYSTEM_MALLOC(size), size);
}

static void SystemFree(
    void* mem)
{
    SYSTEM_FREE(mem);
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


