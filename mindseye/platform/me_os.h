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

