#include "me_memory.h"
#include "core/containers/me_stack.h"
#include "core/me_core.h"
#include "platform/me_os.h"

Allocation meSystemAllocator::meAlloc(u64 size)
{
	Allocation reservation = Allocation(meOSReserveVirtualMemory(size), size);
	meOSCommitVirtualMemory(reservation.data, reservation.size);
	return reservation;
}

Allocation meSystemAllocator::meReserve(u64 size)
{
    return Allocation(meOSReserveVirtualMemory(size), size);
}

void meSystemAllocator::meFree(void* allocation)
{
    meOSFreeVirtualMemory(allocation);
}

void meSystemAllocator::meClear(bool deleteMemory)
{
    // noop, can't really "clear" the system allocations
    UNIMPLEMENTED();
}

meAllocator* GetSystemAllocator()
{
    static meSystemAllocator system = meSystemAllocator();
    return &system;
}

thread_local ArenaScopedScratch scratchWork; // individual systems are in charge of handling their own allocations here.

// TODO: subscribe to thread creation event to release this
MEAPI meAllocator* GetTLScratch()
{
	if (!scratchWork.backing_mem)
	{
		new(&scratchWork)ArenaScopedScratch(ArenaInit(MEGABYTES_BYTES(5), "Threadlocal Scratch", GetSystemAllocator()));
	}
	return &scratchWork;
}


meOwningSpan ReallocateBuffer(
	meAllocator* allocator,
	void* existingBuffer,
	u64 existingBufferSize)
{
	void* newBuffer = MEALLOC(allocator, existingBufferSize);
	ME_MEMCPY(newBuffer, existingBuffer, existingBufferSize);
	return { newBuffer, existingBufferSize };
}

#ifndef ME_CORE_ONLY

#define ENGINE_INITIAL_RESERVED_MEMSIZE GIGABYTES_BYTES(1)

void InitializeAllocatorSystem(EngineContext* engine)
{
    meAllocator* systemAllocator = GetSystemAllocator();
    engine->engineArena = ArenaInit(ENGINE_INITIAL_RESERVED_MEMSIZE, "Engine", systemAllocator);
    engine->engineFrameAllocator = ArenaInit(ENGINE_INITIAL_RESERVED_MEMSIZE, "Engine Frame", systemAllocator);
    engine->engineSceneAllocator = ArenaInit(ENGINE_INITIAL_RESERVED_MEMSIZE, "Engine Scene", systemAllocator);
    engine->gameArena = ArenaInit(ENGINE_INITIAL_RESERVED_MEMSIZE, "Game", systemAllocator);
}

#endif

