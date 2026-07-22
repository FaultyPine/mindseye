// third party

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

// Define STB implementations BEFORE including ImGui to prevent ImGui from defining them again
#define STB_TRUETYPE_IMPLEMENTATION
#define STB_RECT_PACK_IMPLEMENTATION
#include "external/stb/stb_rect_pack.h"
#include "external/stb/stb_truetype.h"

// Now include ImGui - it will skip defining STB implementations since they're already defined
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

// imgui-node-editor
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#define IMGUI_NODE_EDITOR_API MEAPI
// imgui_canvas.cpp and imgui_node_editor_internal.h both define a static
// ImFringeScaleRef helper. In a unity build they collide. Rename the canvas
// copy so the node-editor internal copy (used by FringeScaleScope) wins.
#define ImFringeScaleRef ImFringeScaleRef_Canvas
#include "imgui-node-editor/imgui_canvas.cpp"
#undef ImFringeScaleRef
#include "imgui-node-editor/crude_json.cpp"
#include "imgui-node-editor/imgui_node_editor.cpp"
#include "imgui-node-editor/imgui_node_editor_api.cpp"
#pragma clang diagnostic pop
#include "enkiTS/src/TaskScheduler.cpp"
