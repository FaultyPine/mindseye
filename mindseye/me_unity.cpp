
#define UNITY_BUILD

#include "core/me_defines.h"

#ifdef ME_CORE_ONLY
#define STB_SPRINTF_IMPLEMENTATION
#endif
#define STBSP__PUBLICDEC extern "C" MEAPI
#include "external/stb/stb_sprintf.h"
#ifdef ME_CORE_ONLY
#undef STB_SPRINTF_IMPLEMENTATION
#endif

// HEADER
#include "platform/me_os.h"
#include "core/me_log.h"
#include "core/me_string.h"
#include "core/me_arena.h"
#include "core/me_math.h"
#include "core/me_core.h"
#include "core/me_memory.h"
#include "core/me_event.h"
#include "core/me_filesystem.h"
#include "core/thread/me_thread.h"
#include "core/me_job_system.h"
#include "core/me_scope_exit.h"
#include "core/me_typetraits.h"
#include "core/me_serialize.h"

#include "core/containers/dynarray.h"
#include "core/containers/me_hybrid_array.h"
#include "core/containers/me_blocklist.h"
#include "core/containers/me_map.h"

#ifndef ME_CORE_ONLY
#include "core/me_cmdline.h"
#include "core/me_app.h"
#include "asset/me_asset.h"
#endif

// SOURCE
#ifndef ME_UNITY_HEADER_ONLY
#include "platform/me_os.cpp"
#include "core/me_log.cpp"
#include "core/me_string.cpp"
#include "core/me_arena.cpp"
#include "core/me_math.cpp"
#include "core/me_core.cpp"
#include "core/me_memory.cpp"
#include "core/me_event.cpp"
#include "core/me_filesystem.cpp"
#include "core/thread/me_thread.cpp"
#include "core/me_job_system.cpp"
#include "core/me_serialize.cpp"

#include "core/containers/dynarray.cpp"
#include "core/containers/me_hybrid_array.cpp"
#include "core/containers/me_blocklist.cpp"
#include "core/containers/me_map.cpp"

#include "reflector/reflection_types.cpp"

#ifndef ME_CORE_ONLY
#include "core/me_cmdline.cpp"
#include "core/me_app.cpp"
#include "asset/me_asset.cpp"

// SUBMODULES
#include "render/renderer_unity.cpp"
#include "scene/scene_unity.cpp"

#endif // ME_CORE_ONLY
#endif // ME_UNITY_HEADER_ONLY
