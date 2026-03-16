

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

constexpr u32 WINDOW_CURSOR_WRAP_BUFFER = 2;

void OnResize(HWND hwnd, UINT flag, int width, int height)
{
    EngineContext* ctx = GetEngineCtx();
	if (ctx->osData->onResizeCB)
	{
		ctx->osData->onResizeCB(width, height);
	}
	ctx->osData->windowWidth = width;
	ctx->osData->windowHeight = height;
	LOG_INFO("OnWindowResize OS %ix%i", width, height);
}

//WndProc function
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) 
{
    EngineContext* ctx = GetEngineCtx();
	if (!ctx->osData)
	{
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	OSStateView* osState = ctx->osData;

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
		case WM_KEYDOWN: 
		{
			int virtualKeyCode = (int)wParam;
			osState->keyboardState.keyStates.set(virtualKeyCode, true);
		} break;
		case WM_KEYUP: 
		{
			int virtualKeyCode = (int)wParam;
			osState->keyboardState.keyStates.set(virtualKeyCode, false);
		} break;
		case WM_INPUT:
		{
			if (!osState->useRawInput)
			{
				LOG_WARN("Not using raw input but receiving raw input mouse msgs...");
				break;
			}
			UINT dwSize = sizeof(RAWINPUT);
			static BYTE lpb[sizeof(RAWINPUT)];
			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));
			RAWINPUT* raw = (RAWINPUT*)lpb;
			if (raw->header.dwType == RIM_TYPEMOUSE)
			{
				POINT pos = { (s32)osState->windowWidth / 2, (s32)osState->windowHeight / 2 };
				ClientToScreen(hWnd, &pos);
				SetCursorPos(pos.x, pos.y);
				if (raw->data.mouse.usFlags == MOUSE_MOVE_RELATIVE) 
				{
					glm::vec2 mouseDelta = glm::vec2(raw->data.mouse.lLastX, raw->data.mouse.lLastY);
					osState->mouseState.mouseDelta = mouseDelta;
				} 
				else if (raw->data.mouse.usFlags == MOUSE_MOVE_ABSOLUTE) 
				{
					UNIMPLEMENTED();
				}
			}
			break;
		}
		// TODO: os layer should keep "physical" mouse state
		// rest of mindseye should use a "virtual" mouse state
        case WM_MOUSEMOVE:
        {
			if (osState->useRawInput)
			{
				LOG_WARN("Using raw input but receiving standard mouse msgs?");
				break;
			}
			// Do not use the LOWORD or HIWORD macros to extract the x- and y- coordinates of the cursor position because these macros return incorrect results on systems with multiple monitors. Systems with multiple monitors can have negative x- and y- coordinates, and LOWORD and HIWORD treat the coordinates as unsigned quantities. https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mousemove
			// Supposed to use GET_X_LPARAM and GET_Y_LPARAM, but don't want to bring in another windows header so i've hardcoded those macros in here
			int mouseX = (int)(short)LOWORD(lParam); // GET_X_LPARAM
			int mouseY = (int)(short)HIWORD(lParam); // GET_Y_LPARAM
			osState->mouseState.UpdateMouseScreenPos(mouseX, mouseY);
        }
        break;
        case WM_LBUTTONDOWN: 
        { SET_BIT(osState->mouseState.buttons, meMouseButton::LBUTTON, true); break; }
        case WM_MBUTTONDOWN:
        { SET_BIT(osState->mouseState.buttons, meMouseButton::MBUTTON, true); break; }
        case WM_RBUTTONDOWN:
        { SET_BIT(osState->mouseState.buttons, meMouseButton::RBUTTON, true); break; }
        case WM_LBUTTONUP:
        { SET_BIT(osState->mouseState.buttons, meMouseButton::LBUTTON, false); break; }
        case WM_MBUTTONUP:
        { SET_BIT(osState->mouseState.buttons, meMouseButton::MBUTTON, false); break; }
        case WM_RBUTTONUP:
        { SET_BIT(osState->mouseState.buttons, meMouseButton::RBUTTON, false); break; }
        case WM_MOUSEWHEEL:
        { osState->mouseState.scroll = (int)(short)HIWORD(wParam); break; } // expressed in multiples of WHEEL_DELTA
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}


void meOSCreateWindow(WindowCreationParams creationParams, EngineContext* engine)
{
    const wchar_t* CLASS_NAME  = L"Mindseye";
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
    StringView wideString = StringView((char*)MEALLOC(GetTLScratch(), wide_char_len), wide_char_len);
    MultiByteToWideChar(CP_ACP, 0, creationParams.name.data, -1, (wchar_t*)wideString.data, wide_char_len);

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
		DWORD result = GetLastError();
		LOG_ERROR("Failed to open OS window %i", result);
        return;
    }
    HCURSOR hArrowCursor = LoadCursor(NULL, IDC_ARROW);
    SetCursor(hArrowCursor);

    OSStateView* cachedOSData = &g_osData;
    cachedOSData->hinstance = hInstance;
    cachedOSData->hwnd = hwnd;
    cachedOSData->windowWidth = creationParams.width;
    cachedOSData->windowHeight = creationParams.height;
	QueryPerformanceFrequency((LARGE_INTEGER *)&cachedOSData->ticksPerSecond);
	QueryPerformanceCounter((LARGE_INTEGER *)&cachedOSData->ticksAtAppStart);
    engine->osData = cachedOSData;
    engine->appName = creationParams.name;
}

void meOSTick(EngineContext* engine)
{
    MSG msg;
	OSStateView* osState = engine->osData;

	// tick these before getting os messages
	osState->mouseState.MouseInputTick();
	osState->keyboardState.KeyboardInputTick();

    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) 
    {
        if (msg.message == WM_QUIT)
        {
            engine->isRunning = false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    while (::IsIconic((HWND)osState->hwnd))
    {
        ::Sleep(10);
    }
#if !SHIPPING_BUILD
    if (osState->keyboardState.keyStates.get(VK_ESCAPE))
    {
        engine->isRunning = false;
    }
#endif

	// hitting "tab" in the engine releases/captures your mouse cursor
	if (osState->keyboardState.IsKeyJustPressed(VK_TAB)) 
	{
		bool cursorLocked = engine->osData->useRawInput; // TODO: make this a separate state thingy
        if (!cursorLocked)
		{
            meOSSetCursorState(CAPTURED, *engine->osData);
        }
        else
		{
            meOSSetCursorState(FREE, *engine->osData);
            SetCursorPos(engine->osData->windowWidth / 2.0f, engine->osData->windowHeight / 2.0f);
        }
    }
}

s32 meOSMain(s32 argc, char** argv)
{
    InitializeEngine(argc, argv);
    return 0;
}
#endif

// on windows, reserving memory just means reserving the address space
// you will crash if you r/w out of an address that has only been reserved
void* meOSReserveVirtualMemory(u64 size)
{
    void* result = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
	if (result == nullptr) MEUNLIKELY
	{
		DWORD err = GetLastError();
		LOG_ERROR("meOS ran out of memory! %u", err);
	}
	return result;
}

// on windows, committing a virtual memory range
// that has been reserved means allocating space for it in the page table
// notably, this doesn't mean physical memory is allocated for the range
// that only happens when you actually touch a reserved & "committed" page
void* meOSCommitVirtualMemory(void* ptr, u64 size)
{
    void* result = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
	if (result == nullptr) MEUNLIKELY
	{
		DWORD err = GetLastError();
		LOG_ERROR("meOS ran out of memory! %u", err);
	}
	return result;
}

void meOSFreeVirtualMemory(void* data)
{
	VirtualFree(data, 0, MEM_RELEASE);
}

void* LoadDynamicLibrary(const char* name)
{
    return (void*)LoadLibraryA(name);
}


void* GetFunctionPtr(void* module, StringView functionName)
{
    return (void*)GetProcAddress((HMODULE)module, functionName.cstr());
}


bool meOSReadFileContents(const OSFileReference& file, void* backingBuffer, size_t backingBufferSize)
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

u64 meOSGetFileSize(const OSFileReference& file)
{
    ME_ASSERT(file.fileHandle != nullptr && file.fileHandle != INVALID_HANDLE_VALUE);
    DWORD fileSizeHi = 0;
    DWORD fileSizeLo = GetFileSize(file.fileHandle, &fileSizeHi);
    return (u64)fileSizeLo | ((u64)fileSizeHi << 32);
}


MEAPI FileTimestamps meOSGetFileTimestamps(
	const OSFileReference& file)
{
	FileTimestamps timestamps = {};
    ME_ASSERT(file.fileHandle != nullptr && file.fileHandle != INVALID_HANDLE_VALUE);
	FILETIME lastCreate, lastRead, lastWrite;
	bool result = GetFileTime(file.fileHandle, &lastCreate, &lastRead, &lastWrite);
	ME_ASSERT(result);
	timestamps.created = ((size_t)lastCreate.dwHighDateTime << 32) | (size_t)lastCreate.dwLowDateTime;
	timestamps.lastRead = ((size_t)lastRead.dwHighDateTime << 32) | (size_t)lastRead.dwLowDateTime;
	timestamps.lastWrite = ((size_t)lastWrite.dwHighDateTime << 32) | (size_t)lastWrite.dwLowDateTime;
	return timestamps;
}

bool meOSOpenFile(
	OSFileReference& file, 
	StringView path, 
	OSFileFlags flags)
{
    ME_MEMCLEAR((void*)file.path, PATH_MAX);
    StringCopy({file.path, PATH_MAX}, path);
	flags |= file.flags;
	u32 openMode = OPEN_ALWAYS;
	if (flags & OSFileFlags_OnlyIfExists)
	{
		openMode = flags & OSFileFlags_StompExisting ? TRUNCATE_EXISTING : OPEN_EXISTING;
	}
	else if (flags & OSFileFlags_StompExisting)
	{
		openMode = CREATE_ALWAYS;
	}
    file.fileHandle = CreateFileA(file.path, GENERIC_READ | GENERIC_WRITE, 0 /*exclusive access*/, 0, openMode, FILE_ATTRIBUTE_NORMAL, 0);
    if (file.fileHandle == INVALID_HANDLE_VALUE)
    {
        DWORD result = GetLastError();
        LOG_INFO("[meOS] failed to open file %s err code = %u", file.path, result);
		return false;
    }
    file.flags = flags;
    return true;
}

bool meOSCloseFile(OSFileReference& file)
{
    ME_ASSERT(file.fileHandle != nullptr && file.fileHandle != INVALID_HANDLE_VALUE);
	bool result = CloseHandle(file.fileHandle);
	if (file.flags & OSFileFlags_DeleteOnFileClose)
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

bool meOSReadDirectory(
	const OSFileReference& folder,
	DynArray<OSFileReference>& result)
{
	WIN32_FIND_DATAA ffd;
	StringView folderQuery = StringFormatTmp("%s\\*", folder.path);
	HANDLE hFind = FindFirstFileA(folderQuery.data, &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
	{
        LOG_ERROR("ReadDirectory error %d | Invalid file handle: " STRING_FMT, 
				  GetLastError(), STRING_VAARGS(folderQuery));
		return false;
    }
    do
	{
		StringView filename = StringFromCString(ffd.cFileName, MAX_PATH);
        if (!StringCompare(filename, STRING_LIT(".")) &&
			!StringCompare(filename, STRING_LIT((".."))))
		{
			OSFileReference file = {};
			file.InitWithoutOpening(filename);
			bool isDirectory = ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
			if (isDirectory) file.flags |= OSFileFlags_IsDirectory;
			DynArrayPush(result, file);
        }
    } while (FindNextFileA(hFind, &ffd) != 0);

    DWORD dwError = GetLastError();
    if (dwError != ERROR_NO_MORE_FILES) 
	{
        LOG_ERROR("FindNextFile error. Error is %d\n", dwError);
		return false;
    }

	FindClose(hFind);
	return true;
}

StringView meOSGetExeFilepath()
{
	static char path[PATH_MAX];
	if (path[0] == '\0')
	{
		ME_MEMCLEAR(path, PATH_MAX);
		GetModuleFileNameA(NULL, path, sizeof(path));
		meFsNormalizePathSeperators(StringView(path, CStringLength(path)));
	}
	return StringView(path, CStringLength(path));
}

StringView meOSGetExeFileFolder()
{
	static char path[PATH_MAX];
	if (path[0] == '\0')
	{
		ME_MEMCLEAR(path, PATH_MAX);
		StringView fullPath = meOSGetExeFilepath();
		s32 lastDirSep = FindInStringRev(fullPath, meFsGetDirectorySeperator());
		ME_MEMCPY(path, fullPath.cstr(), lastDirSep);
	}
	return StringView(path, CStringLength(path));
}

StringView meOSGetWorkingDir()
{
	static char workingDirBuffer[PATH_MAX];
	if (workingDirBuffer[0] == '\0')
	{
		ME_MEMCLEAR(workingDirBuffer, PATH_MAX);
		DWORD dwRet = GetCurrentDirectoryA(PATH_MAX, workingDirBuffer);
		if (dwRet == 0) 
		{
			// failure
			DWORD error = GetLastError();
			LOG_ERROR("Failed to get working directory. Err %i", error);
			return {};
		} 
		else if (dwRet > PATH_MAX) 
		{
			// path is too long for the buffer
			ME_ASSERT(false && "working dir is more than PATH_MAX characters");
		}
		StringView result = StringFromCString(workingDirBuffer, PATH_MAX);
		meFsNormalizePathSeperators(result);
		return result;
	}
	StringView result = StringFromCString(workingDirBuffer, PATH_MAX);
	return result;
}

BOOL DirectoryExists(const char* dirPath) {
    DWORD fileAttributes = GetFileAttributesA(dirPath);
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        return FALSE;
    }
    return (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// Function to create a full directory path recursively
bool CreateRecursiveDirectory(StringView path) {
    // Make a mutable copy of the path. MAX_PATH is 260.
    char tempPath[MAX_PATH];
    if (path.len >= MAX_PATH) 
	{
        return false;
    }
	ME_MEMCLEAR(tempPath, MAX_PATH);
    StringCopy(StringView(tempPath, MAX_PATH), path);

    char* p = tempPath;
    
    // Skip past drive letter (e.g., C:\)
    if (p[0] && p[1] == ':' && p[2] == '\\') {
        p += 3;
    }
    
    while (*p) {
        if (*p == '\\' || *p == '/') {
            *p = '\0'; // Temporarily terminate the string
            
            if (!DirectoryExists(tempPath)) {
                if (!CreateDirectoryA(tempPath, NULL)) {
                    DWORD error = GetLastError();
                    if (error != ERROR_ALREADY_EXISTS) {
                        // Handle error.
                        return FALSE;
                    }
                }
            }
            *p = '\\'; // Restore the separator
        }
        p++;
    }

    // Create the final directory in the path
    if (!DirectoryExists(tempPath)) {
        if (!CreateDirectoryA(tempPath, NULL)) {
            DWORD error = GetLastError();
            if (error != ERROR_ALREADY_EXISTS) {
                return false;
            }
        }
    }

    return true;
}

bool meOSEnsureDirectoriesExist(const char* pathCstr)
{
	u64 len = CStringLength(pathCstr);
	return CreateRecursiveDirectory(StringView(pathCstr, len));
}

bool meOSFileExists(
	const OSFileReference& file)
{
	DWORD dwAttrib = GetFileAttributesA(file.path);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
		!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}


bool meOSFileDelete(
	const OSFileReference& file)
{
	bool result = DeleteFileA(file.path);
	if (!result)
	{
		DWORD error = GetLastError();
		LOG_ERROR("Failed to delete file %s %i", file.path, error);
	}
	return result;
}

String meOSResolveRelativeToAbsPath(
	meAllocator* allocator,
	StringView potentiallyRelativePath)
{
	String absolutePath = String(PATH_MAX, allocator);
	ME_MEMCLEAR((char*)absolutePath, PATH_MAX);
	char src[PATH_MAX];
	ME_MEMCLEAR(src, PATH_MAX);
	ME_MEMCPY(src, potentiallyRelativePath.data, potentiallyRelativePath.len);
    DWORD result = GetFullPathNameA(src, PATH_MAX, absolutePath.data, NULL);
	ME_ASSERT(result > 0);
	absolutePath.len = CStringLength(absolutePath.cstr());
	meFsNormalizePathSeperators(absolutePath);
	return meMove(absolutePath);
}

u32 meOSGetThreadID()
{
	return GetCurrentThreadId();
}

u64 OSStateView::GetTicksUsec() const
{
	u64 ticks;
	ME_ASSERT(QueryPerformanceCounter((LARGE_INTEGER*)&ticks));
	ticks -= ticksAtAppStart;
	u64 seconds = ticks / ticksPerSecond;
	u64 leftover = ticks % ticksPerSecond;
	u64 time = (leftover * 1000000L) / ticksPerSecond;
	time += seconds * 1000000L;
	return time;
}

#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC 0x01
#endif

#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE 0x02
#endif

void meOSSetCursorState(
	meOSCursorState state,
	OSStateView& osState)
{
	if (state == CAPTURED)
	{
		HWND hwnd = (HWND)osState.hwnd;

		RAWINPUTDEVICE rid;
		rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
		rid.usUsage = HID_USAGE_GENERIC_MOUSE;
		rid.dwFlags = RIDEV_NOLEGACY; // Exclude legacy WM_MOUSE* messages
		rid.hwndTarget = hwnd;
		if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
		{
			osState.useRawInput = false;
			LOG_WARN("Failed to register raw input device");
		}
		else
		{
			osState.useRawInput = true;
		}

		// Get the client area of your window and convert to screen coordinates
		RECT clientRect;
		GetClientRect(hwnd, &clientRect);
		ClientToScreen(hwnd, (LPPOINT)&clientRect.left);
		ClientToScreen(hwnd, (LPPOINT)&clientRect.right);

		// Clip the cursor to the client area
		//ClipCursor(&clientRect);
		POINT centerPoint = { (s32)osState.windowWidth / 2, (s32)osState.windowHeight / 2 };
		ClientToScreen(hwnd, &centerPoint);
		SetCursorPos(centerPoint.x, centerPoint.y);
		SetCapture(hwnd);

		ShowCursor(false);
	}
	else if (state == FREE)
	{
		RAWINPUTDEVICE Rid[1];

		Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
		Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
		Rid[0].dwFlags = RIDEV_REMOVE;
		Rid[0].hwndTarget = NULL;

		if (!RegisterRawInputDevices(Rid, 1, sizeof(Rid[0])))
		{
			LOG_ERROR("Failed to unregister raw input device");
		}
		else
		{
			osState.useRawInput = false;
		}
		ClipCursor(nullptr);
		ShowCursor(true);
		ReleaseCapture();
	}
}

bool AmIBeingDebugged()
{
	return IsDebuggerPresent();
}
