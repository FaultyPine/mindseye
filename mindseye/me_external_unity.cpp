// third party
// TODO: pch?

#include "core/me_defines.h"

#define STB_SPRINTF_IMPLEMENTATION
#define STBSP__PUBLICDEC extern "C" MEAPI
#include "external/stb/stb_sprintf.h"
#undef STBSP__PUBLICDEC
#undef STB_SPRINTF_IMPLEMENTATION

#define STB_IMAGE_IMPLEMENTATION
#define STBSP_NO_SIMD
#define STBI_NO_SIMD
#define STBIDEF extern "C" MEAPI
#include "external/stb/stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#undef STBIDEF

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-copy-with-user-provided-copy"
#pragma clang diagnostic ignored "-Wcomment"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-function"
#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_API MEAPI
#define IMGUI_DISABLE_SSE

#define STB_TRUETYPE_IMPLEMENTATION
#define STB_RECT_PACK_IMPLEMENTATION
#include "external/stb/stb_rect_pack.h"
#include "external/stb/stb_truetype.h"
#include "bgfx/bgfx/3rdparty/dear-imgui/imgui.h"
#undef IMGUI_INCLUDE_IMGUI_USER_INL
#include "bgfx/bgfx/3rdparty/dear-imgui/imgui.cpp"
#include "bgfx/bgfx/3rdparty/dear-imgui/imgui_draw.cpp"
#include "bgfx/bgfx/3rdparty/dear-imgui/imgui_widgets.cpp"
#include "bgfx/bgfx/3rdparty/dear-imgui/imgui_tables.cpp"
#undef STB_TRUETYPE_IMPLEMENTATION
#undef STB_RECT_PACK_IMPLEMENTATION
#undef STBSP_NO_SIMD
#undef STBI_NO_SIMD
#pragma clang diagnostic pop