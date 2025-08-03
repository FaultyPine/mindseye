
#include "mindseye/core/me_core.h"
#include "mindseye/core/me_log.h"
#include "mindseye/platform/me_os.h"
#include "mindseye/core/me_arena.h"
#include "mindseye/core/me_memory.h"
#include "mindseye/core/me_result.h"

// tests
#include "mindseye/core/containers/dynarray.h"
#include "mindseye/core/containers/me_hybrid_array.h"


void testbed_init(EngineContext* engine, WindowCreationParams& windowCreationParams)
{
    windowCreationParams = {.name = STRING_LIT("Mindseye")};
    DynArrayTests();
    HybridArrayTests();
    int something = sizeof(Result<void, int>);
    UNUSED(something);
    int x = 1;
    UNUSED(x);
    LOG_TRACE("Hello world");
}

void testbed_update(EngineContext* engine)
{
    
}
void testbed_shutdown(EngineContext* engine)
{

}

REGISTER_ME_CALLBACKS(AppCallbacks(testbed_init, testbed_update, testbed_shutdown));