#pragma once

#include "core/me_defines.h"



// The mindseye "handle" type
struct Eye
{
	// currently 24 bits for index, 8 bits for generation
	u32 eye = U32_INVALID_ID;

	static constexpr u32 IndexNumBits = 24;
	static constexpr u32 IndexBitsMask = (1 << IndexNumBits) - 1;
	static constexpr u32 GenerationBitsMask = ((~0U) << (IndexNumBits));

	Eye() = default;
	Eye(u32 idx, u8 generation) 
	{
		eye = 0;
		eye |= (idx & IndexBitsMask);
		eye |= (static_cast<u32>(generation) << IndexNumBits);
	}
	u32 GetIndex() const { return eye & IndexBitsMask; }
	u8 GetGeneration() const { return (eye & GenerationBitsMask) >> IndexNumBits; }
	explicit operator u32() const { return eye; }
	explicit operator bool() const { return eye != U32_INVALID_ID; }
};


// returns the current time since app launch
MEAPI f64 GetTimeUsec();
MEAPI f64 GetTimeSec();

void OverwriteRandomSeed(u64 seed);
u64 GetRandomSeed();
s32 GetRandom(s32 start, s32 end);
f32 GetRandomf(f32 start, f32 end);

#include <type_traits> // For std::remove_reference

template <typename T>
typename std::remove_reference<T>::type&& meMove(T&& arg) noexcept 
{
    return static_cast<typename std::remove_reference<T>::type&&>(arg);
}

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
