#pragma once

#include "core/me_defines.h"


void ConsolePrint(const char* text);


typedef int(*EntryPointFunc)(int argc, char** argv);
MEAPI void __InternalRegisterOSEntryPoint(EntryPointFunc entryPoint);
MEAPI int me_os_platform_main(int argc, char** argv);

// register the *one* entry point of a program
// entry point function must follow EntryPointFunc signature
#define REGISTER_OS_ENTRY_POINT(entryPointFunction) \
    struct ENTRY_POINT_STRUCT { \
        ENTRY_POINT_STRUCT() { __InternalRegisterOSEntryPoint(entryPointFunction); } \
    }; \
    static ENTRY_POINT_STRUCT globalEntryPointHolder = {};\
    int main(int argc, char** argv)\
    {\
        return me_os_platform_main(argc, argv);\
    }\

void* LoadDynamicLibrary(const char* name);
