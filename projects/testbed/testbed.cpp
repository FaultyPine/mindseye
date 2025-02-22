#include "testbed.h"

#include "mindseye/core/me_log.h"
#include "mindseye/me_os_win.h"

int me_main(int argc, char** argv)
{
    InitializeLogger();
    int x = 1;
    REF(x);
    LOG_TRACE("Hello world");
    return 0;
}

REGISTER_OS_ENTRY_POINT(me_main);