#pragma once


#include "me_defines.h"
#include "containers/me_span.h"
struct EngineContext;

C_LINKAGE void* memcpy(void *_Dst, const void *_Src, size_t _Size);
C_LINKAGE void* memmove(void *_Dst, const void *_Src, size_t _Size);
C_LINKAGE void* memset(void *_Dst, int _Val, size_t _Size);
C_LINKAGE int   memcmp(const void *_Buf1, const void *_Buf2, size_t _Size);


#define ME_MEMCPY(dst, src, size) memcpy(dst, src, size)
#define ME_MEMMOVE(dst, src, size) memmove(dst, src, size)
#define ME_MEMCLEAR(dst, size) memset(dst, 0, size)
#define ME_MEMCMP(dst, src, size) memcmp(dst, src, size)


typedef meSpan Allocation;

struct meAllocator
{
    Allocation meAlloc(u64 size) { UNIMPLEMENTED(); }
    Allocation meReserve(u64 size) { UNIMPLEMENTED(); }
    void meFree(void* allocation) { UNIMPLEMENTED(); }
    Allocation meRealloc(const Allocation& allocation, u64 newSize) { UNIMPLEMENTED(); }
    void meClear() { UNIMPLEMENTED(); }

    u64 currentSize = 0;
};

struct meSystemAllocator : public meAllocator
{
    Allocation meAlloc(u64 size);
    Allocation meReserve(u64 size);
    void meFree(void* allocation);
    Allocation meRealloc(const Allocation& allocation, u64 newSize);
    void meClear();
};

meAllocator* GetSystemAllocator();

#define ME_MALLOC(size) GetSystemAllocator()->meAlloc(size)
#define ME_FREE(ptr) GetSystemAllocator()->meFree(ptr)


void InitializeAllocatorSystem(EngineContext* engine);