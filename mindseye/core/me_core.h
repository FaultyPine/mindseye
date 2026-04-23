#pragma once

#include "mindseye/core/me_defines.h"
#include "mindseye/core/me_string.h"
#include <string_view>

// The mindseye runtime "handle" type
// contains an index and generation
struct Eye
{
	// currently 24 bits for index, 8 bits for generation
    #ifdef ME_REFLECTING
    // reflector doesn't currently support bitfields
	u32 eye = 0;
    #else
    union
    {
        u32 eye = 0;
        struct
        {
            u32 index: 23;
            u32 isAssetTemplate: 1;
            u32 generation: 8;
        };
    };
    #endif

	static constexpr u32 IndexNumBits = 23;
    static constexpr u32 AssetTemplateNumBits = 1;
	static constexpr u32 IndexBitsMask = (1 << IndexNumBits) - 1;
	static constexpr u32 GenerationBitsMask = ((~0U) << (IndexNumBits + AssetTemplateNumBits));
    // reserve a bit after the index bits for 
    // "is this Eye for a Template Asset or an Instance Asset?"
    static constexpr u32 AssetTemplateBitMask = (1 << (IndexNumBits + AssetTemplateNumBits)); 

	Eye() = default;
    #ifdef ME_REFLECTING
	Eye(u32 idx, u8 generation, bool isEditor) 
	{
		eye = 0;
		eye |= (idx & IndexBitsMask);
		eye |= (static_cast<u32>(generation) << (IndexNumBits + AssetTemplateNumBits));
        eye |= isEditor ? AssetTemplateBitMask : 0;
	}
	u32 GetIndex() const { return eye & IndexBitsMask; }
	u8 GetGeneration() const { return (eye & GenerationBitsMask) >> (IndexNumBits + AssetTemplateNumBits); }
    bool IsTemplateAsset() const { return eye & AssetTemplateBitMask; }
    #else // ME_REFLECTING
	Eye(u32 idx, u8 generation, bool isAssetTemplate) 
	{
		this->eye = 0;
		this->index = idx;
		this->generation = generation;
        this->isAssetTemplate = isAssetTemplate;
	}
	u32 GetIndex() const { return index; }
	u8 GetGeneration() const { return generation; }
    bool IsTemplateAsset() const { return isAssetTemplate; }
    #endif // ME_REFLECTING
	explicit operator u32() const { return eye; }
    explicit operator u64() const { return (u64)eye; }
	explicit operator bool() const { return eye != 0; }
	bool operator==(const Eye& other) const { return eye == other.eye; }
};
const Eye EYE_INVALID = Eye();

// returns the current time since app launch
MEAPI f64 GetTimeUsec();
MEAPI f64 GetTimeSec();

void OverwriteRandomSeed(u64 seed);
u64 GetRandomSeed();
s32 GetRandom(s32 start, s32 end);
f32 GetRandomf(f32 start, f32 end);


// We need a basic 'remove_reference' trait since we cannot use std::remove_reference
template<typename T>
struct remove_reference {
    using type = T;
};

template<typename T>
struct remove_reference<T&> {
    using type = T;
};

template<typename T>
struct remove_reference<T&&> {
    using type = T;
};

template <typename T>
typename remove_reference<T>::type&& meMove(T&& arg) noexcept 
{
    return static_cast<typename remove_reference<T>::type&&>(arg);
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
constexpr u64 HashStringComptime(std::string_view str) 
{
    u64 hash = 14695981039346656037ull;
    for (char c : str) 
    {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ull;
    }
    return hash;
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


// Assuming floats are in the range [0.0, 1.0] and we want 8 bits per component
u32 PackFloatsToU32(float f1, float f2, float f3, float f4);



template<typename T>
constexpr u64 type_id() {
#if defined(__clang__) || defined(__GNUC__)
    constexpr std::string_view name = __PRETTY_FUNCTION__;
    // Format: constexpr std::size_t type_id() [with T = ...]
    constexpr std::string_view prefix = "T = ";
    auto start = name.find(prefix) + prefix.size();
    auto end = name.find(']', start);
    return HashStringComptime(name.substr(start, end - start));
#elif defined(_MSC_VER)
    constexpr std::string_view name = __FUNCSIG__;
    // Format: size_t __cdecl typeid_util::type_id<...>(void)
    constexpr std::string_view prefix = "type_id<";
    auto start = name.find(prefix) + prefix.size();
    auto end = name.find('>', start);
    return HashStringComptime(name.substr(start, end - start));
#else
    // Fallback: use typeid
    return HashStringComptime(typeid(T).name());
#endif
}

