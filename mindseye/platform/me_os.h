#pragma once

#include "core/me_defines.h"
#include "core/me_arena.h"
#include "core/me_string.h"
struct EngineContext;

struct WindowCreationParams
{
    String name = STRING_LIT("Unknown Window");
    u32 width = 1280;
    u32 height = 720;
};

typedef void(*OnOSWindowResize)(s32 width, s32 height);
typedef void(*OnOSMouseMove)(s32 mx, s32 my);

struct MouseState
{
    s32 mouseX = 0;
    s32 mouseY = 0;
    s32 scroll = 0;
    enum MouseButtons : u32
    {
        LBUTTON,
        RBUTTON,
        MBUTTON,
    };
    u32 buttons = 0;
};

struct OSStateView
{
    // TODO: make these events so multiple systems can subscribe
    OnOSWindowResize onResizeCB = nullptr;
    OnOSMouseMove onMouseMove = nullptr;
    MouseState mouseState = {};
    u32 windowWidth = 0;
    u32 windowHeight = 0; 
#ifdef OS_WINDOWS
    void* hwnd = nullptr;
    void* hinstance = nullptr;
#else

#endif
};

#ifdef OS_WINDOWS
#ifndef PATH_MAX
#define PATH_MAX 260
#endif
#else
#error undefined max path for unk os
#endif

enum OSFileFlags
{
    ScopedFile = NTH_BIT(0)
};

struct OSFileReference
{
    ~OSFileReference();
    OSFileFlags flags = OSFileFlags(0);
    #ifdef OS_WINDOWS
    void* fileHandle = 0;
    char path[PATH_MAX] = {};
    #else
    #error unsupported filereference platform
    #endif
};

void ConsolePrint(const char* text);
void* LoadDynamicLibrary(const char* name);
void* GetFunctionPtr(void* module, String functionName);

MEAPI void meOSCreateWindow(WindowCreationParams creationParams, EngineContext* engine);
MEAPI void meOSTick(EngineContext* engine);
MEAPI void* meOSReserveVirtualMemory(u64 size);
MEAPI const char* meOSFsDirectorySeperator();
MEAPI bool meOSOpenFile(OSFileReference& file, const char* path, OSFileFlags flags = OSFileFlags(0));
MEAPI bool meOSCloseFile(OSFileReference& file);
MEAPI bool meOSReadFileContents(const OSFileReference& file, void* backingBuffer, size_t backingBufferSize);
MEAPI size_t meOSGetFileSize(const OSFileReference& file);
