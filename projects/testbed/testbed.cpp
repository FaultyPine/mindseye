
#include "mindseye/core/me_core.h"
#include "mindseye/core/me_log.h"
#include "mindseye/platform/me_os.h"
#include "mindseye/core/me_arena.h"
#include "mindseye/core/me_memory.h"
#include "mindseye/core/me_result.h"

// tests
#include "mindseye/core/containers/dynarray.h"
#include "mindseye/core/containers/me_hybrid_array.h"


int testbed_main(int argc, char** argv)
{
    EngineContext engine = {};
    InitializeEngine(&engine);
    DynArrayTests();
    HybridArrayTests();
    Arena arena = ArenaInit(500, "somearena"); 
    meOSCreateWindow({.name = STRING_LIT("Mindseye")}, &engine);
    int something = sizeof(Result<void, int>);
    REF(something);
    REF(arena);
    int x = 1;
    REF(x);
    LOG_TRACE("Hello world");
    while (engine.isRunning)
    {
        meOSTick(&engine);
    }
    return 0;
}

REGISTER_OS_ENTRY_POINT(testbed_main);