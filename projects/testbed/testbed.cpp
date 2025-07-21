
#include "mindseye/core/me_core.h"
#include "mindseye/core/me_log.h"
#include "mindseye/platform/me_os.h"
#include "mindseye/core/me_arena.h"
#include "mindseye/core/me_memory.h"
#include "mindseye/core/me_result.h"

int testbed_main(int argc, char** argv)
{
    InitializeEngine();
    Arena arena = ArenaInit(AllocatorGet().alloc(500).data, 500, "somearena"); 
    int something = sizeof(Result<void, int>);
    REF(something);
    REF(arena);
    int x = 1;
    REF(x);
    LOG_TRACE("Hello world");
    return 0;
}

REGISTER_OS_ENTRY_POINT(testbed_main);