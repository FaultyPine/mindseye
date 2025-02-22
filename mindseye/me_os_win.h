#pragma once

#include "core/me_defines.h"
#include "core/me_string.h"

#ifdef OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif



void ConsolePrint(const char* text, u32 textLen);


typedef int(*EntryPointFunc)(int argc, char** argv);
MEAPI void __InternalRegisterOSEntryPoint(EntryPointFunc entryPoint);

// register the *one* entry point of a program
// entry point function must follow EntryPointFunc signature
#define REGISTER_OS_ENTRY_POINT(entryPointFunction) \
    struct ENTRY_POINT_STRUCT { \
        ENTRY_POINT_STRUCT() { __InternalRegisterOSEntryPoint(entryPointFunction); } \
    }; \
    static ENTRY_POINT_STRUCT globalEntryPointHolder = {};



C_LINKAGE MEAPI EntryPointFunc globalEntryPointFunc;


#ifdef OS_WINDOWS
MEAPI int me_os_win_main(int argc, char** argv);
MEAPI int me_os_win_main(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow);

int main(int argc, char** argv)
{
    return me_os_win_main(argc, argv);
}
int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    return me_os_win_main(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}
#endif





