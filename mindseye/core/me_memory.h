#pragma once


#include "me_defines.h"
#include "containers/me_span.h"

C_LINKAGE void* memcpy(void *_Dst, const void *_Src, size_t _Size);
C_LINKAGE void* memmove(void *_Dst, const void *_Src, size_t _Size);
C_LINKAGE void* memset(void *_Dst, int _Val, size_t _Size);
C_LINKAGE int   memcmp(const void *_Buf1, const void *_Buf2, size_t _Size);


#define ME_MEMCPY(dst, src, size) memcpy(dst, src, size)
#define ME_MEMMOVE(dst, src, size) memmove(dst, src, size)
#define ME_MEMCLEAR(dst, size) memset(dst, 0, size)
#define ME_MEMCMP(dst, src, size) memcmp(dst, src, size)


typedef meSpan Allocation;

typedef Allocation(*AllocateFn)(u64 size);
typedef void(*FreeFn)(void* allocation);
typedef Allocation(*ReallocFn)(const Allocation& allocation, u64 newSize);
typedef void(*ClearFn)();

struct meAllocator
{
    AllocateFn alloc;
    FreeFn free;
    ReallocFn realloc;
    ClearFn clear;

    u64 currentSize;
};


MEAPI meAllocator& AllocatorGet();
MEAPI void AllocatorPush(const meAllocator& allocator);
MEAPI meAllocator AllocatorPop();

#define ME_MALLOC(size) AllocatorGet().alloc(size)
#define ME_FREE(ptr) AllocatorGet().free(ptr)


void InitializeAllocatorSystem();