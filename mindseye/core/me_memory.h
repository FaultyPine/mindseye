#pragma once


#include "me_defines.h"
#include "containers/me_span.h"

EXT_IMPORT C_LINKAGE void*  malloc (size_t _Size);
EXT_IMPORT C_LINKAGE void   free   (void *_Block);
EXT_IMPORT C_LINKAGE void*  realloc(void *_Block, size_t newSize);
C_LINKAGE void* memcpy(void *_Dst, const void *_Src, size_t _Size);
C_LINKAGE void* memmove(void *_Dst, const void *_Src, size_t _Size);
C_LINKAGE void* memset(void *_Dst, int _Val, size_t _Size);
C_LINKAGE int   memcmp(const void *_Buf1, const void *_Buf2, size_t _Size);


#define SYSTEM_MALLOC(size) malloc(size)
#define SYSTEM_FREE(ptr) free(ptr)
#define SYSTEM_REALLOC(ptr, newSize) realloc(ptr, newSize)
#define ME_MEMCPY(dst, src, size) memcpy(dst, src, size)
#define ME_MEMMOVE(dst, src, size) memmove(dst, src, size)
#define ME_MEMCLEAR(dst, size) memset(dst, 0, size)
#define ME_MEMCMP(dst, src, size) memcmp(dst, src, size)


typedef meSpan Allocation;

typedef Allocation(*AllocateFn)(u64 size);
typedef void(*FreeFn)(const Allocation& allocation);
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

MEAPI const meAllocator& AllocatorGet();
MEAPI void AllocatorPush(const meAllocator& allocator);
MEAPI meAllocator AllocatorPop();


void InitializeAllocatorSystem();