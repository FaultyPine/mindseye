#pragma once

#include "core/me_defines.h"

// returns the current time since app launch
MEAPI f64 GetTimeUsec();
MEAPI f64 GetTimeSec();

MEAPI void OverwriteRandomSeed(u64 seed);
MEAPI u64 GetRandomSeed();
MEAPI s32 GetRandom(s32 start, s32 end);
MEAPI f32 GetRandomf(f32 start, f32 end);




// Hashing

MEAPI u32 HashBytes(u8* data, u32 size);
MEAPI u64 HashBytesL(u8* data, u32 size);

constexpr u64 FNV_offset_basis_u64 = 0xcbf29ce484222325;
constexpr u64 FNV_prime_u64 = 0x100000001b3;

consteval u64 HashStringComptimeL(const char* str, u64 value = FNV_offset_basis_u64) 
{
    return (str[0] == '\0') ? value : 
		HashStringComptimeL(&str[1], (value ^ static_cast<u64>(str[0])) * FNV_prime_u64);
}
constexpr u32 FNV_offset_basis_u32 = 0x811c9dc5;
constexpr u32 FNV_prime_u32 = 0x01000193;
consteval u32 HashStringComptime(const char* str, u32 value = FNV_offset_basis_u32) 
{
    return (str[0] == '\0') ? value : 
		HashStringComptime(&str[1], (value ^ static_cast<u32>(str[0])) * FNV_prime_u32);
}

template <typename T>
void HashCombineImpl(u64& seed, const T& val) {
    seed ^= HashBytesL((u8*)&val, sizeof(T)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename... Types>
u64 HashCombine(const Types&... args) {
    u64 seed = 0;
    (HashCombineImpl(seed, args), ...); 
    return seed;
}
