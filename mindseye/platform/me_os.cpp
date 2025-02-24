


#include "core/me_defines.h"
#include <stdio.h> // _vfprintf_l

#ifdef OS_WINDOWS
#include "windows/me_os_win.cpp"
#endif




static EntryPointFunc globalEntryPointFunc = nullptr;


void __InternalRegisterOSEntryPoint(EntryPointFunc entryPoint)
{
    if (globalEntryPointFunc)
    {
        LOG_ERROR("Cannot register OS entry point more than once!");
    }
    globalEntryPointFunc = entryPoint;
}

void ConsolePrint(const char* text, ...) 
{
    int res;
    va_list arglist;
    __crt_va_start(arglist, text);
    res = _vfprintf_l(stdout, text, NULL, arglist);
    __crt_va_end(arglist);
    ME_ASSERT(res > 0);
    REF(res);
}

