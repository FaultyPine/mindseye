#include "core/me_defines.h"
#define STB_SPRINTF_IMPLEMENTATION
#include "external/stb/stb_sprintf.h"
#ifndef ME_CORE_ONLY
#define STB_TRUETYPE_IMPLEMENTATION
#define STB_RECT_PACK_IMPLEMENTATION
#include "external/stb/stb_rect_pack.h"
#include "external/stb/stb_truetype.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#define STBIDEF extern "C" MEAPI
#include "external/stb/stb_image.h"
#endif
