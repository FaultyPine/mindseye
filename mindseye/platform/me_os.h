#pragma once

#include "core/me_defines.h"
#include "core/me_arena.h"
#include "core/me_string.h"
struct EngineContext;

struct WindowCreationParams
{
    String name = STRING_LIT("Unknown Window");
    u32 width = 800;
    u32 height = 600;
};

struct OSCookbook
{
#ifdef OS_WINDOWS
    void* hwnd;
    void* hinstance;
#else

#endif
};

void ConsolePrint(const char* text);
void* LoadDynamicLibrary(const char* name);
void* GetFunctionPtr(void* module, String functionName);

MEAPI void meOSCreateWindow(WindowCreationParams creationParams, EngineContext* engine);
MEAPI void meOSTick(EngineContext* engine);
MEAPI void* meOSReserveVirtualMemory(u64 size);

typedef int(*EntryPointFunc)(int argc, char** argv);
MEAPI void __InternalRegisterOSEntryPoint(EntryPointFunc entryPoint);

// register the *one* entry point of a program
// entry point function must follow EntryPointFunc signature
#define REGISTER_OS_ENTRY_POINT(entryPointFunction) \
    struct ENTRY_POINT_STRUCT { \
        ENTRY_POINT_STRUCT() { __InternalRegisterOSEntryPoint(entryPointFunction); } \
    }; \
    static ENTRY_POINT_STRUCT globalEntryPointHolder = {};
