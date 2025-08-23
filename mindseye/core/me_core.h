#pragma once

#include "core/me_defines.h"

// returns the current time since app launch
MEAPI f64 GetTime();
// just casts GetTime to f32
MEAPI f32 GetTimef();

MEAPI void OverwriteRandomSeed(u64 seed);
MEAPI u64 GetRandomSeed();
MEAPI s32 GetRandom(s32 start, s32 end);
MEAPI f32 GetRandomf(f32 start, f32 end);

MEAPI u32 HashBytes(u8* data, u32 size);
MEAPI u64 HashBytesL(u8* data, u32 size);

template <typename T>
void HashCombineImpl(u64& seed, const T& val) {
    seed ^= std::hash<T>()(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename... Types>
u64 HashCombine(const Types&... args) {
    u64 seed = 0;
    (HashCombineImpl(seed, args), ...); 
    return seed;
}
