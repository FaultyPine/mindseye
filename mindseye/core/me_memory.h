#pragma once


#include "me_defines.h"
#include "containers/me_span.h"
#include <new> // for placement new
struct EngineContext;

C_LINKAGE void* memcpy(void *_Dst, const void *_Src, size_t _Size);
C_LINKAGE void* memmove(void *_Dst, const void *_Src, size_t _Size);
C_LINKAGE void* memset(void *_Dst, int _Val, size_t _Size);
C_LINKAGE int   memcmp(const void *_Buf1, const void *_Buf2, size_t _Size);


#define ME_MEMCPY(dst, src, size) memcpy(dst, src, size)
#define ME_MEMMOVE(dst, src, size) memmove(dst, src, size)
#define ME_MEMCLEAR(dst, size) memset(dst, 0, size)
#define ME_MEMSET(dst, val, size) memset(dst, val, size)
#define ME_MEMCMP(dst, src, size) memcmp(dst, src, size)


typedef meSpan Allocation;

struct meAllocator
{
    virtual Allocation meAlloc(u64 size) { UNIMPLEMENTED(); }
    virtual Allocation meReserve(u64 size) { return meAlloc(size); }
    virtual void meFree(void* allocation) { UNIMPLEMENTED(); }
    virtual Allocation meRealloc(const Allocation& allocation, u64 newSize) { UNIMPLEMENTED(); }
    virtual void meClear() { UNIMPLEMENTED(); }

    u64 currentSize = 0;
};

struct meSystemAllocator : public meAllocator
{
    MEAPI Allocation meAlloc(u64 size) override;
    MEAPI Allocation meReserve(u64 size) override;
    MEAPI void meFree(void* allocation) override;
    MEAPI Allocation meRealloc(const Allocation& allocation, u64 newSize) override;
    MEAPI void meClear() override;
};


meAllocator* GetSystemAllocator();

#define MESYSMALLOC(size) GetSystemAllocator()->meAlloc(size)
#define MESYSFREE(ptr) GetSystemAllocator()->meFree(ptr)

#define MEALLOC(allocator, size) ((allocator)->meAlloc(size))
#define MEFREE(allocator, data) ((allocator)->meFree(data))

#define MENEW(allocator, Type, ...) \
    new (MEALLOC((allocator), sizeof(Type)).data) Type(__VA_ARGS__)

#define MEDELETE(allocator, Type, data) \
do { \
    if (data) { \
        data->~Type(); \
        MEFREE(allocator, data); \
    } \
} while(0)

#ifdef COMPILER_CLANG
#define MSB64(x) (63 - __builtin_clzll(x))
#define MSB32(x) (31 - __builtin_clzll(x))
#else
#error need manual support for MSB on non clang compiler
#endif

void InitializeAllocatorSystem(EngineContext* engine);