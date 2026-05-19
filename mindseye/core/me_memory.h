#pragma once


#include "me_defines.h"
#include "containers/me_span.h"
#include "mindseye/core/me_string.h"
#include <new> // for placement new

#define ME_MEM_DEBUG false

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
	virtual Allocation meAlloc(u64 size) { UNIMPLEMENTED(); return {}; }
    virtual Allocation meReserve(u64 size) { return meAlloc(size); }
    virtual void meFree(void* allocation) { UNIMPLEMENTED(); }
	virtual Allocation meRealloc(const Allocation& allocation, u64 newSize) 
	{
		// default impl just copies...
		Allocation result = meAlloc(newSize);
		ME_MEMCPY(result.data, allocation.data, allocation.size);
		return result;
	}
    virtual void meClear(bool deleteMemory = false) { UNIMPLEMENTED(); }

	meAllocator() : name(STRING_LIT("Unnamed allocator")) {}
	meAllocator(StringView allocatorName) : name(allocatorName) {}
	StringView name;
};

struct meSystemAllocator : public meAllocator
{
    MEAPI Allocation meAlloc(u64 size) override;
    MEAPI Allocation meReserve(u64 size) override;
    MEAPI void meFree(void* allocation) override;
    MEAPI void meClear(bool deleteMemory = false) override;

	meSystemAllocator() : meAllocator(STRING_LIT("System Allocator")) {}
};

struct ScopedAllocation
{
    Allocation allocation;
    meAllocator* allocator;
    ScopedAllocation(meAllocator* allocator, u64 size);
    ~ScopedAllocation();
};

MEAPI meAllocator* GetSystemAllocator();
MEAPI meAllocator* GetDefaultAllocator();
MEAPI meAllocator* GetTLScratch();

// takes an existing buffer and allocates + copies it into a new allocation
MEAPI meOwningSpan ReallocateBuffer(
	meAllocator* allocator,
	void* existingBuffer,
	u64 existingBufferSize);
MEAPI bool BufferCopy(meSpan dst, meSpan src);


#define MESYSMALLOC(size) GetSystemAllocator()->meAlloc(size)
#define MESYSFREE(ptr) GetSystemAllocator()->meFree(ptr)

#define MEALLOC(allocator, size) ((allocator)->meAlloc(size))
#define MEREALLOC(allocator, buf, size) ((allocator)->meRealloc(buf, size))
#define MERESERVE(allocator, size) ((allocator)->meReserve(size))
#define MEFREE(allocator, ptr) \
	do { \
		if (ptr != nullptr) { \
			(allocator)->meFree(ptr); \
			ptr = {}; \
		} \
	} while(0)

inline Allocation _internalAllocAndClear(meAllocator* allocator, u64 size)
{
    Allocation alloc = MEALLOC(allocator, size);
    ME_MEMCLEAR(alloc, size);
    return alloc;
}
#define MECALLOC(allocator, size) (_internalAllocAndClear(allocator, size))

#define MENEW(allocator, Type, ...) \
    (new (MEALLOC((allocator), sizeof(Type)).data) Type(__VA_ARGS__))

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

struct meCursorBuffer
{
    const u8* data = nullptr;
    u32 size = 0;
    u32 cursor = 0;

    bool read(void* out, u32 len)
    {
        if (cursor + len > size) return false;
        ME_MEMCPY(out, data + cursor, len);
        cursor += len;
        return true;
    }
    template<typename T> bool readT(T& out) { return read(&out, sizeof(T)); }
    void skip(u32 n) { cursor += n; }
    bool eof() const { return cursor >= size; }
};

#ifndef ME_CORE_ONLY

// TODO: allocator handles

struct EngineContext;
void InitializeAllocatorSystem(EngineContext* engine);
#endif