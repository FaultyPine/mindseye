

// third party
// TODO: pch?

#include "core/me_defines.h"

#define STB_SPRINTF_IMPLEMENTATION
#define STBSP__PUBLICDEC extern "C" MEAPI
#include "external/stb/stb_sprintf.h"
#undef STBSP__PUBLICDEC
#undef STB_SPRINTF_IMPLEMENTATION

#define STB_IMAGE_IMPLEMENTATION
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
#include "imgui\imgui.h"
#include "imgui\imgui.cpp"
#include "imgui\imgui_draw.cpp"
#include "imgui\imgui_widgets.cpp"
#include "imgui\imgui_tables.cpp"
#pragma clang diagnostic pop