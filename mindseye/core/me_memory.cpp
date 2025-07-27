#include "me_memory.h"
#include "core/containers/me_stack.h"
#include "core/me_core.h"
#include "platform/me_os.h"

EXT_IMPORT C_LINKAGE void*  malloc (size_t _Size);
EXT_IMPORT C_LINKAGE void   free   (void *_Block);
EXT_IMPORT C_LINKAGE void*  realloc(void *_Block, size_t newSize);
#define SYSTEM_MALLOC(size) malloc(size)
#define SYSTEM_FREE(ptr) free(ptr)
#define SYSTEM_REALLOC(ptr, newSize) realloc(ptr, newSize)

Allocation SystemAlloc(
    u64 size)
{
    return Allocation((u8*)SYSTEM_MALLOC(size), size);
}

Allocation SystemReserve(
    u64 size)
{
    return Allocation(meOSReserveVirtualMemory(size), size);
}

void SystemFree(
    void* mem)
{
    SYSTEM_FREE(mem);
}

Allocation SystemRealloc(
    const Allocation& mem, 
    u64 newSize)
{
    return Allocation((u8*)SYSTEM_REALLOC(mem.data, newSize), newSize);
}

meAllocator* GetSystemAllocator()
{
    static meAllocator system;
    if (system.alloc == nullptr)
    {
        system.alloc = SystemAlloc;
        system.realloc = SystemRealloc;
        system.reserve = SystemReserve;
        system.free = SystemFree;
        system.currentSize = 0;
    }
    return &system;
}

#define ENGINE_INITIAL_RESERVED_MEMSIZE GIGABYTES_BYTES(1)

void InitializeAllocatorSystem(EngineContext* engine)
{
    meAllocator* systemAllocator = GetSystemAllocator();
    engine->engineArena = ArenaInit(ENGINE_INITIAL_RESERVED_MEMSIZE, "Engine", systemAllocator->reserve(ENGINE_INITIAL_RESERVED_MEMSIZE));
    engine->engineFrameAllocator = ArenaInit(ENGINE_INITIAL_RESERVED_MEMSIZE, "Engine Frame", systemAllocator->reserve(ENGINE_INITIAL_RESERVED_MEMSIZE));
    engine->engineSceneAllocator = ArenaInit(ENGINE_INITIAL_RESERVED_MEMSIZE, "Engine Scene", systemAllocator->reserve(ENGINE_INITIAL_RESERVED_MEMSIZE));
    engine->gameArena = ArenaInit(ENGINE_INITIAL_RESERVED_MEMSIZE, "Game", systemAllocator->reserve(ENGINE_INITIAL_RESERVED_MEMSIZE));
    engine->scratchWork = ArenaInit(ENGINE_INITIAL_RESERVED_MEMSIZE, "Scratch", systemAllocator->reserve(ENGINE_INITIAL_RESERVED_MEMSIZE));
}



