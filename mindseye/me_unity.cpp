
#define UNITY_BUILD

#include "core/me_defines.h"

#define STBSP__PUBLICDEC extern "C" MEAPI
#include "external/stb/stb_sprintf.h"

// HEADER
#include "platform/me_os.h"
#include "core/me_log.h"
#include "core/me_string.h"
#include "core/me_arena.h"
#include "core/me_math.h"
#include "core/me_core.h"
#include "core/me_memory.h"

#include "core/containers/dynarray.h"
#include "core/containers/me_hybrid_array.h"
#include "core/containers/me_blocklist.h"


// SOURCE
#include "platform/me_os.cpp"
#include "core/me_log.cpp"
#include "core/me_string.cpp"
#include "core/me_arena.cpp"
#include "core/me_math.cpp"
#include "core/me_core.cpp"
#include "core/me_memory.cpp"

#include "core/containers/dynarray.cpp"
#include "core/containers/me_hybrid_array.cpp"
#include "core/containers/me_blocklist.cpp"

// SUBMODULES
#include "render/renderer_unity.cpp"
#include "scene/scene_unity.cpp"
