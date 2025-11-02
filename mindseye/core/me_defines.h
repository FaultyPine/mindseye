#ifndef TINY_DEFINES_H
#define TINY_DEFINES_H

typedef unsigned char u8;
typedef char s8;
typedef unsigned short u16;
typedef short s16;
typedef unsigned int u32;
typedef int s32;
typedef long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef wchar_t wchar;


// assertions only in debug mode
#if BUILD_DEBUG
#define ME_ASSERTIONS_ENABLED
#endif

#define UNUSED(x) (void)(x)
#define UNUSED_DECL [[maybe_unused]]

#if defined(__clang__)
#define COMPILER_CLANG
#elif defined(__GNUC__)
#define COMPILER_GCC
#elif defined(_MSC_VER)
#define COMPILER_MSVC
#else
#error "Unrecognized compiler
#endif

#if defined(_WIN32)
#undef OS_WINDOWS
#define OS_WINDOWS
#elif defined(__linux__)
#undef OS_LINUX
#define OS_LINUX
#else
#error "Unrecogized platform"
#endif


#if defined(COMPILER_CLANG) || defined(COMPILER_GCC)
#define STATIC_ASSERT _Static_assert
#else
#define STATIC_ASSERT static_assert
#endif

#if __cplusplus < 202002L
#error Expected C++ 20 standard or above
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#define NODISCARD [[nodiscard]]

#define MELIKELY [[likely]]
#define MEUNLIKELY [[unlikely]]

#define KILOBYTES_BYTES(kb) (kb*1024)
#define MEGABYTES_BYTES(mb) (mb*KILOBYTES_BYTES(1024))
#define GIGABYTES_BYTES(gb) (gb*MEGABYTES_BYTES(1024))

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) ( sizeof((arr))/sizeof((arr)[0]) )
#endif

#define CLAMP(x, min, max) (x < min ? min : (x > max ? max : x))

#define SET_BIT(bitfield, n_bit, onoff) \
    (bitfield = ((bitfield) & ~((u32)1 << (n_bit))) | ((u32)(onoff) << (n_bit)) )

#define TOGGLE_NTH_BIT(bitfield, n_bit) \
    ( (bitfield) ^ (1 << (n_bit)) )

#define TEST_BIT(bitfield, n_bit) ((( bitfield >> n_bit ) & 1U) == 1)

#define NTH_BIT(index) (1 << index)

#define MEMIN(a, b) (a > b ? b : a)
#define MEMAX(a, b) (a > b ? a : b)

// concat tokens without expanding macro definitions
#define ME_MACRO_CONCAT(a,b) a##b
// concat tokens after macro expanding them
#define ME_MACRO_CONCAT_EX(a,b) TMACRO_CONCAT(a,b)

// stringize token without macro expanding A
#define ME_MACRO_STRINGIZE(A) #A
// stringize token after macro expanding A
#define ME_MACRO_STRINGIZE_EX(A) ME_MACRO_STRINGIZE(A)

// clang annotation
#define MEANNOTATE(str) __attribute__((annotate(str)))

#define ME_REFLECT_ATTR_STR "mereflect"
// op is an indicator to the reflection system of what the next stuff in the macro will be
// like   MEREFLECT(type, someType)
// or     MEREFLECT(exclude)
// TODO: document these options once they're properly implemented
#define MEREFLECT(op, ...) MEANNOTATE(ME_REFLECT_ATTR_STR) 

#define U32_INVALID_ID 0xFFFFFFFF
#define U64_INVALID_ID 0xFFFFFFFFFFFFFFFF
#define ME_EPSILON FLT_EPSILON

#ifdef COMPILER_MSVC
#define EXT_IMPORT __declspec(dllimport)
#elif defined(COMPILER_CLANG)
#define EXT_IMPORT __attribute__((dllimport))
#else
#define EXT_IMPORT
#endif

#ifdef COMPILER_MSVC
#define EXT_EXPORT __declspec(dllexport)
#elif defined(COMPILER_CLANG)
#define EXT_EXPORT __attribute__((dllexport))
#else
#define EXT_EXPORT __attribute__((visibility("default")))
#endif

// exports
#ifdef MEEXPORT
#define MEAPI EXT_EXPORT
// imports
#else 
#define MEAPI EXT_IMPORT
#endif

#define C_LINKAGE extern "C"

#ifdef COMPILER_MSVC
#define ME_ALIGN(n) __declspec(align(n))
#else
#define ME_ALIGN(n) __attribute__((aligned(n)))
#endif

#ifdef COMPILER_MSVC
#define ME_INLINE __forceinline
#define ME_NOINLINE __declspec(noinline)
#else
#define ME_INLINE static inline
#define ME_NOINLINE
#endif


#ifdef COMPILER_MSVC
C_LINKAGE void __cdecl __debugbreak(void);
#define DEBUG_BREAK __debugbreak()
#else
#define DEBUG_BREAK __builtin_trap()
#endif

// TODO: crash report/dump. 
#ifdef ME_ASSERTIONS_ENABLED
    #ifdef LOG_FATAL
        #define ME_ASSERT(x) \
            if (!(x)) MEUNLIKELY { LOG_FATAL("%s | %s:%i", #x, __FILE__, __LINE__); DEBUG_BREAK; }
    #else
        #define ME_ASSERT(x) \
            if (!(x)) MEUNLIKELY { DEBUG_BREAK; }
    #endif
    #define UNIMPLEMENTED() ME_ASSERT(!"Unimplemented!");
#else
    #define ME_ASSERT(x) UNUSED(x)
    #define UNIMPLEMENTED()
#endif

#ifdef LOG_FATAL
#define ME_RTASSERT(x) \
if (!(x)) MEUNLIKELY { LOG_FATAL("%s | %s:%i", #x, __FILE__, __LINE__); DEBUG_BREAK; }
#else
#define ME_RTASSERT(x) \
if (!(x)) MEUNLIKELY { DEBUG_BREAK; }
#endif

#define BREAKABLE_SCOPE do {

#define BREAKABLE_SCOPE_END } while (false);

// counts zeroes going "left-to-right" (most significant -> least significant bits)
#if defined(COMPILER_CLANG) || defined(COMPILER_GCC)
#define CLZ32(x) ((x) == 0 ? 32 : __builtin_clz(x))
#elif defined(COMPILER_MSVC)
#include <intrin.h>
static inline int __bsr_clz32(uint32_t x) 
{
	unsigned long index;
	_BitScanReverse(&index, x);
	return 31 - index;
}
#define CLZ32(x) ((x) == 0 ? 32 : __bsr_clz32(x))
#endif
// counts zeroes going "right-to-left" (least significant -> most significant bits)
#if defined(COMPILER_CLANG) || defined(COMPILER_GCC)
#define CTZ32(x) ((x) == 0 ? 32 : __builtin_ctz(x))
#elif defined(COMPILER_MSVC)
static inline int __bsr_ctz32(uint32_t x) 
{
	unsigned long index;
	_BitScanForward(&index, x);
	return index;
}
#define CTZ32(x) ((x) == 0 ? 32 : __bsr_ctz32(x))
#endif

#endif