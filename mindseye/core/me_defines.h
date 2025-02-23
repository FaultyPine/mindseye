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

#define ME_DEBUG

// assertions only in debug mode
#ifdef ME_DEBUG
#define ME_ASSERTIONS_ENABLED
#endif

#define REF(x) (void)(x)

#if defined(__clang__) && !defined(COMPILER_CLANG)
#define COMPILER_CLANG
#endif
#if defined(__GNUC__) && !defined(COMPILER_GCC)
#define COMPILER_GCC
#endif

#if defined(_WIN32) && !defined(OS_WINDOWS)
#define OS_WINDOWS
#endif
#if defined(COMPILER_CLANG) && defined(__linux__) && !defined(OS_LINUX)
#define OS_LINUX
#endif

#if defined(__clang__) || defined(__GNUC__)
#define STATIC_ASSERT _Static_assert
#else
#define STATIC_ASSERT static_assert
#endif

#define ME_NODISCARD [[nodiscard]]

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) ( sizeof((arr))/sizeof((arr)[0]) )
#endif

#define SET_NTH_BIT(bitfield, n_bit, onoff) \
    (bitfield = (bitfield & ~((u32)1 << n_bit)) | ((u32)onoff << n_bit) )

#define TOGGLE_NTH_BIT(bitfield, n_bit) \
    ( (bitfield) ^ (1 << (n_bit)) )

#define CHECK_NTH_BIT(bitfield, n_bit) ((( bitfield >> n_bit ) & 1U) == 1)

// concat tokens without expanding macro definitions
#define ME_MACRO_CONCAT(a,b) a##b
// concat tokens after macro expanding them
#define ME_MACRO_CONCAT_EX(a,b) TMACRO_CONCAT(a,b)

// stringize token without macro expanding A
#define ME_MACRO_STRINGIZE(A) #A
// stringize token after macro expanding A
#define ME_MACRO_STRINGIZE_EX(A) TMACRO_STRINGIZE(A)

#define U32_INVALID_ID 999999999U

// exports
#ifdef MEEXPORT
#ifdef _MSC_VER
#define MEAPI __declspec(dllexport)
#else
#define MEAPI __attribute__((visibility("default")))
#endif
// imports
#else 
#ifdef _MSC_VER
#define MEAPI __declspec(dllimport)
#else
#define MEAPI
#endif
#endif

#define C_LINKAGE extern "C"

#ifdef _MSC_VER
#define ME_ALIGN(n) __declspec(align(n))
#else
#define ME_ALIGN(n) __attribute__((aligned(n)))
#endif

#ifdef _MSC_VER
#define ME_INLINE __forceinline
#define ME_NOINLINE __declspec(noinline)
#else
#define ME_INLINE static inline
#define ME_NOINLINE
#endif

#endif