#pragma once

#include "me_defines.h"
// ARENAS

struct Arena 
{
    unsigned char* backing_mem = 0;
    size_t backing_mem_size = 0;
    size_t offset = 0;
    size_t prev_offset = 0;
};

#define ArenaAllocType(arena, type, num) ((type*)ArenaAlloc(arena, sizeof(type) * num))
#define ARENA_MAX_NAME_LEN 30

MEAPI Arena ArenaInit(void* backingBuffer, size_t arenaSize);
MEAPI Arena ArenaInit(void* backingBuffer, size_t arenaSize, const char* name);
MEAPI void* ArenaAlloc(Arena* arena, size_t allocSize);
MEAPI void* ArenaResize(Arena* arena, void* oldMem, size_t oldSize, size_t newSize);
MEAPI void ArenaClear(Arena* arena);
MEAPI void ArenaClearNull(Arena* arena);
MEAPI void ArenaFreeAll(Arena* arena);
MEAPI const char* ArenaGetName(Arena* arena);

struct ArenaTemp 
{
    Arena* arena;
    size_t prev_offset;
    size_t offset;
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

MEAPI ArenaTemp ArenaTempInit(Arena* arena);
MEAPI void ArenaTempEnd(ArenaTemp tmpArena);

