#pragma once

#include "me_defines.h"
#include "me_memory.h"

#define ArenaAllocType(arena, type, num) ((type*)ArenaAlloc(arena, sizeof(type) * num))
#define ARENA_MAX_NAME_LEN 30

// TODO: make this chained & threadsafe.
// Actually. Don't make this chained. Just do an OS virtual mem reserve of 1gb.
// Nearly all systems reserve ~48 bits for pointers
// which represents 262,144GB. >200k arenas is way way more than enough...
struct Arena : public meAllocator 
{
    unsigned char* backing_mem = 0;
    size_t backing_mem_size = 0;
    size_t offset = 0;
    size_t prev_offset = 0;
    meAllocator* backingAllocator = nullptr;

    MEAPI Allocation meAlloc(u64 size) override;
    MEAPI void meFree(void* allocation) override;
    MEAPI Allocation meRealloc(const Allocation& allocation, u64 newSize) override;
    MEAPI void meClear() override;
};

MEAPI Arena ArenaInit(size_t arenaSize, const char* name = nullptr, meAllocator* backingAllocator = nullptr);
MEAPI void* ArenaAlloc(Arena* arena, size_t allocSize);
MEAPI void* ArenaResize(Arena* arena, void* oldMem, size_t oldSize, size_t newSize);
MEAPI void ArenaClear(Arena* arena);
MEAPI void ArenaClearNull(Arena* arena);
MEAPI void ArenaFreeAll(Arena* arena);
MEAPI const char* ArenaGetName(Arena* arena);

// TODO: (and note to self)
// Make these temp arena funcs take in a 
// list of "persistent" existing arenas
// so we don't get conflicts.
// I tried using these without that feature, being very aware of this pitfall,
// and still fell into it a bunch of times. Use with caution! (until I implement the conflicts thing)
struct ArenaTemp 
{
    Arena* arena;
    size_t prev_offset;
    size_t offset;
	operator Arena*() const { return arena; }
};

inline void* ArenaAlloc(ArenaTemp* arena, size_t alloc_size) 
{
    // when using temp arenas, use the underlying arena
    return ArenaAlloc(arena->arena, alloc_size);
}
inline void* ArenaResize(ArenaTemp* arena, void* oldMem, size_t oldSize, size_t newSize) 
{
    return ArenaResize(arena->arena, oldMem, oldSize, newSize);
}

// NOTE: be extremely careful with these temp arenas
// Initializing one, then doing an allocation with the
// backing arena meant to be tied to that original one will
// cause that allocation to be "wiped out" when the temp arena ends...
// Rule of thumb: tmp arenas should have tiny scope, and no allocations from other arenas
// should be used during the lifetime of a tmp arena
MEAPI ArenaTemp ArenaTempInit(Arena* arena);
MEAPI void ArenaTempEnd(ArenaTemp tmpArena);


struct ArenaTempScoped : public ArenaTemp
{
	~ArenaTempScoped()
	{
		ArenaTempEnd(*this);
	}
};
