
#define UNITY_BUILD

#include "core/me_defines.h"

// Make STB functions static in this compilation unit to avoid conflicts
// The real implementations (with external linkage) are in me_external_unity.cpp
#define STBRP_STATIC
#define STBTT_STATIC
#define STBRP_ASSERT(x)
#define STBTT_assert(x)

// STB_SPRINTF_IMPLEMENTATION is now in me_external_unity.cpp
#define STBSP__PUBLICDEC extern "C" MEAPI
#include "external/stb/stb_sprintf.h"

// HEADER
#include "platform/me_os.h"
#include "platform/me_input.h"
#include "core/me_log.h"
#include "core/me_compile_run_smoke_test.h"
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
#include "core/me_chunker.h"
#include "core/me_relptr.h"

#include "core/containers/dynarray.h"
#include "core/containers/me_hybrid_array.h"
#include "core/containers/me_blocklist.h"
#include "core/containers/me_map.h"

#ifndef ME_CORE_ONLY
#include "core/me_serialize.h"
#include "core/me_cmdline.h"
#include "core/me_command.h"
#include "core/me_app.h"
#include "asset/me_asset.h"
#include "core/me_resourcepool.h"
#endif

// SOURCE
#if !defined(ME_REFLECTING) // reflector doesn't need to parse source files

#include "platform/me_os.cpp"
#include "platform/me_input.cpp"
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
#include "core/me_chunker.cpp"

#include "core/containers/dynarray.cpp"
#include "core/containers/me_hybrid_array.cpp"
#include "core/containers/me_blocklist.cpp"
#include "core/containers/me_map.cpp"

#include "reflector/reflection_types.cpp"

#ifndef ME_CORE_ONLY
#include "core/me_serialize.cpp"
#include "core/me_cmdline.cpp"
#include "core/me_command.cpp"
#include "core/me_app.cpp"
#include "asset/me_asset.cpp"
#include "asset/me_asset_index.cpp"

#include "generatedtypes/generatedtypes_unity_sources.generated.cpp"
#endif // ME_CORE_ONLY

#endif


#ifndef ME_CORE_ONLY

// SUBMODULES
#include "render/renderer_unity.cpp"
#include "scene/scene_unity.cpp"
#include "editor/me_editor_unity.cpp"

#endif // ME_CORE_ONLY
