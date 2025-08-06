//#include "pch.h"
#include "me_arena.h"
#include "core/me_log.h"
#include "core/me_string.h"
#include "core/me_memory.h"

Allocation Arena::meAlloc(u64 size) { return {ArenaAlloc(this, size), size}; }
void Arena::meFree(void* allocation) { ME_ASSERT(backing_mem <= allocation && allocation <= (backing_mem + backing_mem_size)); }
Allocation Arena::meRealloc(const Allocation& allocation, u64 newSize) { UNIMPLEMENTED(); }
void Arena::meClear() { ArenaClear(this); }

Arena ArenaInit(size_t arena_size, const char* name, meAllocator* backingAllocator) 
{
    Arena a;
    meAllocator* allocator = backingAllocator != nullptr ? backingAllocator : GetSystemAllocator();
    a.backingAllocator = allocator;
    a.backing_mem = (unsigned char*)MEALLOC(allocator, arena_size).data;
    a.backing_mem_size = arena_size;
    a.offset = 0;
    a.prev_offset = 0;
    //ME_MEMCLEAR(backing_buffer, arena_size);
    if (name != nullptr)
    {
        char* name_mem = (char*)ArenaAlloc(&a, ARENA_MAX_NAME_LEN); 
        ME_MEMCLEAR(name_mem, ARENA_MAX_NAME_LEN);
        StringCopy(FromCString(name_mem, ARENA_MAX_NAME_LEN), FromCString(name, ARENA_MAX_NAME_LEN));
    }
    return a;
}

const char* ArenaGetName(Arena* arena) 
{
    const char* possible_string = (const char*)arena->backing_mem;
    for (int i = 0; i < ARENA_MAX_NAME_LEN; i++) {
        if (possible_string[i] == '\0') {
            return possible_string;
        }
    }
    return "UNNAMED_ARENA";
}

void* ArenaAlloc(Arena* arena, size_t alloc_size) 
{
    size_t& offset = arena->offset;
    bool is_out_of_mem = offset + alloc_size > arena->backing_mem_size;
    if (is_out_of_mem) 
    {
        LOG_FATAL("Out of memory in arena %s\n", ArenaGetName(arena));
        // maybe we automatically resize here?
        return nullptr;
    }
    // TODO: enforce alignment    
    void* new_alloc = arena->backing_mem + offset;
    arena->prev_offset = offset;
    offset += alloc_size;
    return new_alloc;
}

void* ArenaResize(Arena* arena, void* old_mem, size_t old_size, size_t new_size) 
{
    // resize memory block if it's the most recent alloc.
    // otherwise, resizing just means reallocating and copying old mem to new spot
    uintptr_t old_mem_addr = (uintptr_t)old_mem;
    uintptr_t backing_mem_addr = (uintptr_t)arena->backing_mem;
    bool is_old_mem_in_range = old_mem_addr >= backing_mem_addr && old_mem_addr < backing_mem_addr + arena->offset;
    if (is_old_mem_in_range) 
    {
        bool is_most_recent_alloc = old_mem_addr == backing_mem_addr + arena->prev_offset;
        if (is_most_recent_alloc) {
            arena->offset = arena->prev_offset + new_size;
            return old_mem;
        }
        else 
        {
            void* new_mem = ArenaAlloc(arena, new_size);
            size_t copy_size = old_size < new_size ? old_size : new_size; // smaller of the two
            ME_MEMMOVE(new_mem, old_mem, copy_size);
            return new_mem;
        }
    }
    else 
    {
        ME_ASSERT(false && "Out of bounds resize in arena");
        return nullptr;
    }
}

void ArenaClear(Arena* arena) 
{
    arena->offset = 0;
    arena->prev_offset = 0;
}

void ArenaClearNull(Arena* arena) 
{
    arena->offset = 0;
    arena->prev_offset = 0;
    ME_MEMCLEAR(arena->backing_mem, arena->backing_mem_size);
}


void ArenaFreeAll(Arena* arena)
{
    ArenaClear(arena);
    arena->backing_mem_size = 0;
    MEFREE(arena->backingAllocator, arena->backing_mem);
}

ArenaTemp ArenaTempInit(Arena* arena) 
{
    ArenaTemp tmp;
    tmp.arena = arena;
    tmp.offset = arena->offset;
    tmp.prev_offset = arena->prev_offset;
    return tmp;
}
void ArenaTempEnd(ArenaTemp tmp_arena) 
{
    tmp_arena.arena->offset = tmp_arena.offset;
    tmp_arena.arena->prev_offset = tmp_arena.prev_offset;
}


