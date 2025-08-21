
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
#include "core/me_cmdline.h"
#include "core/me_event.h"
#include "core/me_filesystem.h"
#include "core/thread/me_thread.h"
#include "core/me_job_system.h"

#include "asset/me_asset.h"

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
#include "core/me_cmdline.cpp"
#include "core/me_event.cpp"
#include "core/me_filesystem.cpp"
#include "core/thread/me_thread.cpp"
#include "core/me_job_system.cpp"

#include "asset/me_asset.cpp"

#include "core/containers/dynarray.cpp"
#include "core/containers/me_hybrid_array.cpp"
#include "core/containers/me_blocklist.cpp"

// SUBMODULES
#include "render/renderer_unity.cpp"
#include "scene/scene_unity.cpp"
