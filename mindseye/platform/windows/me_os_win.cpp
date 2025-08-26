

#include "core/me_defines.h"
#include "platform/me_os.h"
#include "core/me_memory.h"
#include "core/me_log.h"

#ifndef OS_WINDOWS
#error "Including windows header in non windows build!
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

#ifndef ME_CORE_ONLY
#include "core/me_app.h"

void OnResize(HWND hwnd, UINT flag, int width, int height)
{
    EngineContext* ctx = GetEngineCtx();
    if (ctx->osData)
    {
        if (ctx->osData->onResizeCB)
        {
            ctx->osData->onResizeCB(width, height);
        }
        ctx->osData->windowWidth = width;
        ctx->osData->windowHeight = height;
    }
    LOG_INFO("OnWindowResize OS %ix%i", width, height);
}

void OnMouseMove(HWND hwnd, UINT flag, int mx, int my)
{
    EngineContext* ctx = GetEngineCtx();
    if (ctx->osData)
    {
        if (ctx->osData->onMouseMove)
        {
            ctx->osData->onMouseMove(mx, my);
        }
        ctx->osData->mouseState.mouseX = mx;
        ctx->osData->mouseState.mouseY = my;
    }
}

//WndProc function
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) 
{
    EngineContext* ctx = GetEngineCtx();
    switch(msg) 
    {
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
        case WM_MOUSEMOVE:
        {
            // Do not use the LOWORD or HIWORD macros to extract the x- and y- coordinates of the cursor position because these macros return incorrect results on systems with multiple monitors. Systems with multiple monitors can have negative x- and y- coordinates, and LOWORD and HIWORD treat the coordinates as unsigned quantities. https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mousemove
            // Supposed to use GET_X_LPARAM and GET_Y_LPARAM, but don't want to bring in another windows header so i've hardcoded those macros in here
            int mouseX = (int)(short)LOWORD(lParam); // GET_X_LPARAM
            int mouseY = (int)(short)HIWORD(lParam); // GET_Y_LPARAM
            OnMouseMove(hWnd, wParam, mouseX, mouseY);
        }
        break;
        case WM_LBUTTONDOWN: 
        { SET_BIT(ctx->osData->mouseState.buttons, MouseState::LBUTTON, true); break; }
        case WM_MBUTTONDOWN:
        { SET_BIT(ctx->osData->mouseState.buttons, MouseState::MBUTTON, true); break; }
        case WM_RBUTTONDOWN:
        { SET_BIT(ctx->osData->mouseState.buttons, MouseState::RBUTTON, true); break; }
        case WM_LBUTTONUP:
        { SET_BIT(ctx->osData->mouseState.buttons, MouseState::LBUTTON, false); break; }
        case WM_MBUTTONUP:
        { SET_BIT(ctx->osData->mouseState.buttons, MouseState::MBUTTON, false); break; }
        case WM_RBUTTONUP:
        { SET_BIT(ctx->osData->mouseState.buttons, MouseState::RBUTTON, false); break; }
        case WM_MOUSEWHEEL:
        { ctx->osData->mouseState.scroll = (int)(short)HIWORD(wParam); break; } // expressed in multiples of WHEEL_DELTA
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

    // When you create a window, windows immediately fires a WM_SIZE event
    // this can create a discrepency between some window sizing logic.
    // We "adjust" the desired window size here to account for the Title Bar/any other window decorations
    // so all systems see the same window size 
    RECT rect;
    rect.left = 0;
    rect.top = 0;
    rect.right = creationParams.width;
    rect.bottom = creationParams.height;
    DWORD dwStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    AdjustWindowRectEx(&rect, dwStyle, 0, 0);

    HWND hwnd = CreateWindowEx(
        0, // Optional window styles.
        CLASS_NAME,
        (wchar_t*)wideString.data,
        dwStyle,

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, 
        rect.right - rect.left,      // Calculated Width
        rect.bottom - rect.top,      // Calculated Height,

        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
        );

    if (hwnd == NULL)
    {
        return;
    }
    HCURSOR hArrowCursor = LoadCursor(NULL, IDC_ARROW);
    SetCursor(hArrowCursor);

    OSStateView* cachedOSData = MENEW(&engine->engineArena, OSStateView);
    cachedOSData->hinstance = hInstance;
    cachedOSData->hwnd = hwnd;
    cachedOSData->windowWidth = creationParams.width;
    cachedOSData->windowHeight = creationParams.height;
    engine->osData = cachedOSData;
    engine->appName = creationParams.name;
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
    while (::IsIconic((HWND)engine->osData->hwnd))
    {
        ::Sleep(10);
    }
#if !SHIPPING_BUILD
    if (GetKeyState(VK_ESCAPE) & 0x8000)
    {
        engine->isRunning = false;
    }
#endif
}

s32 meOSWinMain(s32 argc, char** argv)
{
    InitializeEngine(argc, argv);
    return 0;
}
#endif


void* meOSWinReserveVirtualMemory(u64 size)
{
    void* result = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
	if (result == nullptr) MEUNLIKELY
	{
		DWORD err = GetLastError();
		LOG_ERROR("meOS ran out of memory! %u", err);
	}
	return result;
}

void* meOSWinCommitVirtualMemory(u64 size)
{
    void* result = VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (result == nullptr) MEUNLIKELY
	{
		DWORD err = GetLastError();
		LOG_ERROR("meOS ran out of memory! %u", err);
	}
	return result;
}

void meOSWinFreeVirtualMemory(void* data)
{
	VirtualFree(data, 0, MEM_RELEASE);
}

void* LoadDynamicLibrary(const char* name)
{
    return (void*)LoadLibraryA(name);
}


void* GetFunctionPtr(void* module, String functionName)
{
    return (void*)GetProcAddress((HMODULE)module, functionName.data);
}


bool meOSWinReadFileContents(const OSFileReference& file, void* backingBuffer, size_t backingBufferSize)
{
    ME_ASSERT(file.fileHandle != nullptr && file.fileHandle != INVALID_HANDLE_VALUE);
    DWORD numBytesRead = 0;
    if (!ReadFile(file.fileHandle, backingBuffer, backingBufferSize, &numBytesRead, nullptr))
    {
        DWORD result = GetLastError();
        LOG_ERROR("[meOS] failed to read file contents %s err code = %u", file.path, result);
		return false;
    }
    return numBytesRead == backingBufferSize;
}

bool meOSWriteFileContent(
	const OSFileReference& file,
	void* buffer,
	size_t amtToWrite)
{
	DWORD amtActuallyWritten = 0;
	bool result = WriteFile(file.fileHandle, buffer, amtToWrite, &amtActuallyWritten, nullptr);
	return result;
}

u64 meOSWinGetFileSize(const OSFileReference& file)
{
    ME_ASSERT(file.fileHandle != nullptr && file.fileHandle != INVALID_HANDLE_VALUE);
    DWORD fileSizeHi = 0;
    DWORD fileSizeLo = GetFileSize(file.fileHandle, &fileSizeHi);
    return (u64)fileSizeLo | ((u64)fileSizeHi << 32);
}

bool meOSWinOpenFile(OSFileReference& file, const char* path, OSFileFlags flags)
{
    ME_MEMCLEAR((void*)file.path, PATH_MAX);
    StringCopy({file.path, PATH_MAX}, StringFromCString(path));
    file.flags = (OSFileFlags)((u32)file.flags | (u32)flags);
	u32 openMode = (flags & OnlyIfExists) ? OPEN_EXISTING : OPEN_ALWAYS;
    file.fileHandle = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0 /*exclusive access*/, 0, openMode, FILE_ATTRIBUTE_NORMAL, 0);
    if (file.fileHandle == INVALID_HANDLE_VALUE)
    {
        DWORD result = GetLastError();
        LOG_INFO("[meOS] failed to open file %s err code = %u", path, result);
		return false;
    }
    return true;
}

bool meOSWinCloseFile(OSFileReference& file)
{
    ME_ASSERT(file.fileHandle != nullptr && file.fileHandle != INVALID_HANDLE_VALUE);
	bool result = CloseHandle(file.fileHandle);
	if (file.flags & DeleteOnFileClose)
	{
		meOSDeleteFile(file);
	}
	file.fileHandle = nullptr;
    return result;
}

bool meOSDeleteFile(
	OSFileReference& file)
{
	bool result = DeleteFileA(file.path);
	return result;
}

char* meOSGetExeFilepath()
{
	static char path[PATH_MAX];
	ME_MEMCLEAR(path, PATH_MAX);
	GetModuleFileNameA(NULL, path, sizeof(path));
	return path;
}

char* meOSGetExeFileFolder()
{
	static char path[PATH_MAX];
	ME_MEMCLEAR(path, PATH_MAX);
	char* fullPath = meOSGetExeFilepath();
	s32 lastDirSep = FindInStringRev(StringView(fullPath, PATH_MAX), STRING_LIT("\\"));
	ME_MEMCPY(path, fullPath, lastDirSep);
	return path;
}


char* meOSResolveRelativeToAbsPath(
	meAllocator* allocator,
	StringView potentiallyRelativePath)
{
	char* absolutePath = MEALLOC(allocator, PATH_MAX);
	ME_MEMCLEAR(absolutePath, PATH_MAX);
    DWORD result = GetFullPathNameA(potentiallyRelativePath.data, PATH_MAX, absolutePath, NULL);
	ME_ASSERT(result > 0);
	return absolutePath;
}
