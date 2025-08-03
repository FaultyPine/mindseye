

#include "core/me_defines.h"
#include "platform/me_os.h"
#include "core/me_core.h"

#ifndef OS_WINDOWS
#error "Including windows header in non windows build!
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN


void OnResize(HWND hwnd, UINT flag, int width, int height)
{
    // Handle resizing
    LOG_INFO("Resized window to %ix%i", width, height);
}

//WndProc function
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) 
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hDc = BeginPaint(hWnd, &ps);

            FillRect(hDc, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW + 1));

            EndPaint(hWnd, &ps);

            return 0;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }
        case WM_SIZE:
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            OnResize(hWnd, (UINT)wParam, width, height);
        }
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}


void meOSWinCreateWindow(WindowCreationParams creationParams, EngineContext* engine)
{
    const wchar_t* CLASS_NAME  = L"Mindseye";
    ArenaTemp scratch = ArenaTempInit(&engine->scratchWork);
    WNDCLASS wc = {};

    // "Passing 0 retrieves the handle of the calling process, not the calling module. 
    // If the library/framework is implemented as a DLL, you would end up with the wrong handle. 
    // Use the handle passed to DllMain() or DllEntryPoint() instead
    HINSTANCE hInstance = GetModuleHandle(0);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    int wide_char_len = MultiByteToWideChar(CP_UTF8, 0, creationParams.name.data, -1, nullptr, 0);
    String wideString = String((char*)ArenaAlloc(&scratch, wide_char_len), wide_char_len);
    MultiByteToWideChar(CP_UTF8, 0, creationParams.name.data, -1, (wchar_t*)wideString.data, wide_char_len);
    HWND hwnd = CreateWindowEx(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        (wchar_t*)wideString.data,      // Window text
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,            // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, 
        creationParams.width, creationParams.height,

        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
        );

    if (hwnd == NULL)
    {
        return;
    }
    
    OSCookbook* cachedOSData = ArenaAllocType(&engine->engineArena, OSCookbook, 1);
    cachedOSData->hinstance = hInstance;
    cachedOSData->hwnd = hwnd;
    engine->osData = cachedOSData;
}

void meOSWinTick(EngineContext* engine)
{
    MSG msg;
    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) 
    {
        if (msg.message == WM_QUIT)
        {
            engine->isRunning = false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void* meOSWinReserveVirtualMemory(u64 size)
{
    return VirtualAlloc(nullptr, size, MEM_RESERVE, 0);
}

int meOSWinMain(int argc, char** argv)
{
    InitializeEngine();
    return 0;
}


void* LoadDynamicLibrary(const char* name)
{
    return (void*)LoadLibraryA(name);
}


void* GetFunctionPtr(void* module, String functionName)
{
    return (void*)GetProcAddress((HMODULE)module, functionName.data);
}

