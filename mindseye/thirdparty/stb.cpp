#include "core/me_defines.h"
#include "core/me_memory.h"
#define STB_SPRINTF_IMPLEMENTATION
#include "external/stb/stb_sprintf.h"
#ifndef ME_CORE_ONLY
#define STB_TRUETYPE_IMPLEMENTATION
#define STB_RECT_PACK_IMPLEMENTATION
#include "external/stb/stb_rect_pack.h"
#include "external/stb/stb_truetype.h"

// #define STBI_ASSERT(x) ME_ASSERT(x)
// #define STBI_MALLOC(sz)           (MEALLOC(GetDefaultAllocator(), sz).data)
// #define STBI_REALLOC_SIZED(p,oldsz,newsz) (MEREALLOC(GetDefaultAllocator(), Allocation(p, oldsz), newsz).data)
// #define STBI_FREE(p)              MEFREE(GetDefaultAllocator(), p)

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#define STBIDEF extern "C" MEAPI
#include "external/stb/stb_image.h"
#endif
