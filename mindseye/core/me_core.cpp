#include "me_core.h"

#include "me_app.h"
#include "platform/me_os.h"

// sanity
STATIC_ASSERT(sizeof(s8) == 1);
STATIC_ASSERT(sizeof(u8) == 1);
STATIC_ASSERT(sizeof(s16) == 2);
STATIC_ASSERT(sizeof(u16) == 2);
STATIC_ASSERT(sizeof(u32) == 4);
STATIC_ASSERT(sizeof(s32) == 4);
STATIC_ASSERT(sizeof(u64) == 8);
STATIC_ASSERT(sizeof(s64) == 8);
STATIC_ASSERT(sizeof(f32) == 4);
STATIC_ASSERT(sizeof(f64) == 8);
STATIC_ASSERT(sizeof(void*) == 8);

void OverwriteRandomSeed(u64 seed) 
{
    UNIMPLEMENTED();

    // globEngineCtx.randomSeed = seed;
}
u64 GetRandomSeed() 
{ 
    UNIMPLEMENTED();

    // return globEngineCtx.randomSeed; 
    return 0;
}
s32 GetRandom(s32 start, s32 end) 
{
    UNIMPLEMENTED();

    // lazy init random seed
    // if (globEngineCtx.randomSeed == 0) {
    //     // truly random initial seed. Subsequent random calls simply increment the seed deterministically
    //     f64 time = GetTime();
    //     globEngineCtx.randomSeed = Math::hash((const char*)&time, sizeof(f64));
    //     //std::cout << "Initial random seed = " << randomSeed << "";
    // }
    // srand(Math::hash((const char*)&globEngineCtx.randomSeed, sizeof(globEngineCtx.randomSeed)));
    // globEngineCtx.randomSeed++; // deterministic random
    // return start + (rand() % end);
    return 0;
}
f32 GetRandomf(f32 start, f32 end) 
{
    UNIMPLEMENTED();

    // lazy init random seed
    // if (globEngineCtx.randomSeed == 0) 
    // {
    //     // truly random initial seed. Subsequent random calls simply increment the seed deterministically
    //     f64 time = GetTime();
    //     globEngineCtx.randomSeed = Math::hash((const char*)&time, sizeof(f64));
    //     //std::cout << "Initial random seed = " << randomSeed << "";
    // }
    // // deterministic random
    // u32 newSeed = Math::hash((const char*)&globEngineCtx.randomSeed, sizeof(globEngineCtx.randomSeed));
    // srand(newSeed);
    // globEngineCtx.randomSeed++; 
    // f32 zeroToOneRandom = ((f32)rand()) / RAND_MAX;
    // return Math::Lerp(start, end, zeroToOneRandom);
    return 0.0;
}

// returns the current time since app launch
f64 GetTimeUsec() 
{
	f64 result = GetEngineCtx()->osData->GetTicksUsec();
    return result;
}

f64 GetTimeSec()
{
	f64 result = GetTimeUsec() / 1000000.0;
	return result;
}


u64 HashBytesL(u8* data, u32 size)
{
    // FNV-1 hash
    // https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
    u64 hash = FNV_offset_basis_u64;
    for (u32 i = 0; i < size; i++)
    {
        u8 byte_of_data = data[i];
        hash = hash ^ byte_of_data;
        hash = hash * FNV_prime_u64;
    }
    return hash;
}

u32 HashBytes(u8* data, u32 size)
{
    // FNV-1 hash -> doing the *PRIME, and THEN the XOR.
    // FNV-1a hash -> doing the XOR, then the *PRIME
    // https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function

    u32 hash = FNV_offset_basis_u32;
    for (u32 i = 0; i < size; i++)
    {
        u8 byte_of_data = data[i];
        hash = hash ^ byte_of_data;
        hash = hash * FNV_prime_u32;
    }
    return hash;
}


// Assuming floats are in the range [0.0, 1.0] and we want 8 bits per component
u32 PackFloatsToU32(float f1, float f2, float f3, float f4) 
{
    // Quantize each float to an 8-bit integer (0-255)
    u32 val1 = static_cast<u32>(floor(f1 * 255.0f));
    u32 val2 = static_cast<u32>(floor(f2 * 255.0f));
    u32 val3 = static_cast<u32>(floor(f3 * 255.0f));
    u32 val4 = static_cast<u32>(floor(f4 * 255.0f));

    // Pack into a single u32
    // Assuming order: f1 (LSB), f2, f3, f4 (MSB)
    u32 packed_value = (val4 << 24) | (val3 << 16) | (val2 << 8) | val1;
    return packed_value;
}
