

#include "core/me_defines.h"
#include "platform/me_os.h"

#ifndef OS_WINDOWS
#error "Including windows header in non windows build!
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

C_LINKAGE MEAPI EntryPointFunc globalEntryPointFunc;

int me_os_win_main(int argc, char** argv)
{
    return globalEntryPointFunc ? globalEntryPointFunc(argc, argv) : 1;
}


void* LoadDynamicLibrary(const char* name)
{
    return (void*)LoadLibraryA(name);
}