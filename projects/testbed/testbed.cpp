#include "testbed.h"

#include "mindseye/core/me_log.h"
#include "mindseye/platform/me_os.h"
#include "mindseye/core/me_arena.h"
#include "mindseye/core/me_memory.h"
#include "mindseye/core/me_result.h"
#include "mindseye/core/containers/dynarray.h"
#include "mindseye/core/containers/blocklist.h"
#include "mindseye/core/containers/fixed_growable_array.h"

int me_main(int argc, char** argv)
{
    InitializeLogger();
    Arena arena = ArenaInit(SYSTEM_MALLOC(500), 500, "somearena"); 
    int something = sizeof(Result<void, int>);
    REF(something);
    REF(arena);
    int x = 1;
    REF(x);
    LOG_TRACE("Hello world");
    return 0;
}

REGISTER_OS_ENTRY_POINT(me_main);