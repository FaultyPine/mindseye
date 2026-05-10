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

	Arena() = default;
	Arena(u64 size, const char* name, meAllocator* backingAllocator = nullptr);

    MEAPI Allocation meAlloc(u64 size) override;
    MEAPI void meFree(void* allocation) override;
    MEAPI Allocation meRealloc(const Allocation& allocation, u64 newSize) override;
    MEAPI void meClear(bool deleteMemory = false) override;
};

// Specifically for thread-local scratch memory
struct ArenaTLScratch : public Arena
{
	ArenaTLScratch() = default;
	ArenaTLScratch(const Arena&& arena)
	{
		backing_mem = arena.backing_mem;
		backing_mem_size = arena.backing_mem_size;
		offset = arena.offset;
		prev_offset = arena.prev_offset;
		backingAllocator = arena.backingAllocator;
	}
	ArenaTLScratch(const ArenaTLScratch& other) = delete; // copy
	ArenaTLScratch& operator=(const ArenaTLScratch& other) = delete; // copy assignment

	MEAPI void meClear(bool deleteMemory = false) override;
};

MEAPI Arena ArenaInit(
	size_t arenaSize,
	const char* name = nullptr,
	meAllocator* backingAllocator = nullptr);
MEAPI void ArenaInit(
	Arena& a,
	size_t arenaSize,
	const char* name = nullptr,
	meAllocator* backingAllocator = nullptr);
// Initialise an arena over memory you already own (e.g. a memory-mapped file).
// The arena does NOT free this memory — the caller is responsible for its lifetime.
MEAPI void ArenaInitFromMemory(Arena& a, void* mem, size_t size, const char* name = nullptr);
MEAPI void* ArenaAlloc(Arena* arena, size_t allocSize);
MEAPI void* ArenaResize(Arena* arena, void* oldMem, size_t oldSize, size_t newSize);
MEAPI void ArenaClear(Arena* arena);
MEAPI void ArenaClearNull(Arena* arena);
MEAPI void ArenaFreeAll(Arena* arena);
MEAPI const char* ArenaGetName(Arena* arena);
u64 ArenaGetFreeSpace(Arena* arena) { return arena->backing_mem_size - arena->offset; }

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
