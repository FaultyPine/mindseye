#pragma once

#include "core/me_defines.h"

#ifdef OS_WINDOWS
#include "windows/me_os_win.h"
#endif



void ConsolePrint(const char* text, ...);



typedef int(*EntryPointFunc)(int argc, char** argv);
MEAPI void __InternalRegisterOSEntryPoint(EntryPointFunc entryPoint);

// register the *one* entry point of a program
// entry point function must follow EntryPointFunc signature
#define REGISTER_OS_ENTRY_POINT(entryPointFunction) \
    struct ENTRY_POINT_STRUCT { \
        ENTRY_POINT_STRUCT() { __InternalRegisterOSEntryPoint(entryPointFunction); } \
    }; \
    static ENTRY_POINT_STRUCT globalEntryPointHolder = {};
