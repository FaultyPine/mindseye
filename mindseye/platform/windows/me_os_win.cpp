
#define ME_CRASHHANDLER_AUTO_ZIP 0

#include "core/me_defines.h"
#include "platform/me_os.h"
#include "core/me_memory.h"
#include "core/me_log.h"
#include "core/me_profile.h"
#ifndef ME_CORE_ONLY
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "external/miniz/miniz.h"
#undef MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#endif

#ifndef OS_WINDOWS
#error "Including windows header in non windows build!
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef ME_CORE_ONLY
#include <dbghelp.h>
#endif
#undef WIN32_LEAN_AND_MEAN

#ifndef ME_CORE_ONLY
static constexpr ULONG ME_CRASH_HANDLER_CONTEXT_STREAM_TYPE = 0x4d450001; // Must be > LastReservedStream.

static meCrashHandlerContext g_crashHandlerContext = {};

static StringView CrashFileName(StringView path)
{
    s32 lastForwardSlash = FindInStringRev(path, STRING_LIT("/"), 0, StringOpFlags_IdxAfterNeedle);
    s32 lastBackSlash = FindInStringRev(path, STRING_LIT("\\"), 0, StringOpFlags_IdxAfterNeedle);
    s32 fileNameStart = MEMAX(lastForwardSlash, lastBackSlash);
    return fileNameStart == -1 ? path : path.OffsetView(fileNameStart);
}

static StringBuilder CrashCreateStackTraceBuilder()
{
    static char stackTraceBuffer[64 * 1024];
    StringBuilder builder = {};
    builder.data = stackTraceBuffer;
    builder.len = 0;
    builder.capacity = sizeof(stackTraceBuffer);
    builder.allocator = GetDefaultAllocator();
    return builder;
}

static bool CrashCreateZip(StringView zipPath, StringView* filePaths, u32 fileCount)
{
    mz_zip_archive zip = {};
    if (!mz_zip_writer_init_file(&zip, zipPath.cstr(), 0))
        return false;

    u32 writtenEntries = 0;
    for (u32 i = 0; i < fileCount; i++)
    {
        StringView filePath = filePaths[i];
        if (!filePath)
            continue;

        if (mz_zip_writer_add_file(&zip, CrashFileName(filePath).cstr(), filePath.cstr(), nullptr, 0, MZ_BEST_COMPRESSION))
            writtenEntries++;
    }

    bool ok = writtenEntries > 0 && mz_zip_writer_finalize_archive(&zip);
    mz_zip_writer_end(&zip);
    return ok;
}

static void CrashAppendStackTrace(EXCEPTION_POINTERS* exceptionInfo, StringBuilder& stackTrace)
{
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, nullptr, TRUE);

    CONTEXT context = *exceptionInfo->ContextRecord;
    STACKFRAME64 frame = {};
    DWORD machineType = 0;

#if defined(_M_X64) || defined(__x86_64__)
    machineType = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context.Rip;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrStack.Offset = context.Rsp;
#elif defined(_M_IX86) || defined(__i386__)
    machineType = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context.Eip;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrStack.Offset = context.Esp;
#else
    stackTrace.Append(STRING_LIT("Stack walking is not implemented for this CPU architecture.\n"));
    return;
#endif

    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    for (u32 frameIdx = 0; frameIdx < 128; frameIdx++)
    {
        if (!StackWalk64(machineType, process, thread, &frame, &context, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
            break;
        if (frame.AddrPC.Offset == 0)
            break;

        DWORD64 displacement = 0;
        char symbolStorage[sizeof(SYMBOL_INFO) + 512] = {};
        SYMBOL_INFO* symbol = (SYMBOL_INFO*)symbolStorage;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 511;

        IMAGEHLP_LINE64 line = {};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;

        bool hasSymbol = SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol) != FALSE;
        bool hasLine = SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisplacement, &line) != FALSE;

        if (hasSymbol && hasLine)
        {
            stackTrace.Append(StringFormatTmp("#%02u 0x%llx %s + 0x%llx (%s:%lu)\n",
                frameIdx, frame.AddrPC.Offset, symbol->Name, displacement, line.FileName, line.LineNumber));
        }
        else if (hasSymbol)
        {
            stackTrace.Append(StringFormatTmp("#%02u 0x%llx %s + 0x%llx\n",
                frameIdx, frame.AddrPC.Offset, symbol->Name, displacement));
        }
        else
        {
            stackTrace.Append(StringFormatTmp("#%02u 0x%llx\n", frameIdx, frame.AddrPC.Offset));
        }
    }
}

static bool CrashWriteDump(EXCEPTION_POINTERS* exceptionInfo, StringView dumpPath, const meCrashHandlerContext& crashContext)
{
    HANDLE file = CreateFileA(dumpPath.cstr(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    MINIDUMP_EXCEPTION_INFORMATION dumpException = {};
    dumpException.ThreadId = GetCurrentThreadId();
    dumpException.ExceptionPointers = exceptionInfo;
    dumpException.ClientPointers = FALSE;

    MINIDUMP_TYPE dumpType = (MINIDUMP_TYPE)(
        MiniDumpWithDataSegs |
        MiniDumpWithHandleData |
        MiniDumpWithUnloadedModules |
        MiniDumpWithIndirectlyReferencedMemory |
        MiniDumpWithThreadInfo);

    MINIDUMP_USER_STREAM userStream = {};
    userStream.Type = ME_CRASH_HANDLER_CONTEXT_STREAM_TYPE;
    userStream.BufferSize = sizeof(crashContext);
    userStream.Buffer = (void*)&crashContext;

    MINIDUMP_USER_STREAM_INFORMATION userStreams = {};
    userStreams.UserStreamCount = 1;
    userStreams.UserStreamArray = &userStream;

    bool ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, dumpType, &dumpException, &userStreams, nullptr) != FALSE;
    CloseHandle(file);
    return ok;
}

static LONG WINAPI meUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo)
{
    bool isDebugging = IsDebuggerPresent();

    meCrashHandlerContext crashContext = g_crashHandlerContext;

    StringView exeFolder = meOSGetExeFileFolder();

    char reportsDir[ME_PATH_MAX] = {};
    StringCopy(StringView(reportsDir, ME_PATH_MAX), StringFormatTmp(STRING_FMT "\\crash_reports", STRING_VAARGS(exeFolder)));
    StringView reportsDirView = StringFromCString(reportsDir);
    CreateDirectoryA(reportsDirView.cstr(), nullptr);

    SYSTEMTIME time = {};
    GetLocalTime(&time);
    DWORD pid = GetCurrentProcessId();

    StringView crashFolderName;
    if (isDebugging)
    {
        // if we crash in debug mode, we still capture a dump, but only the latest so we don't fill the disk
        crashFolderName = StringFormatTmp("mindseye_crash_debugging_latest");
    }
    else
    {
        crashFolderName = StringFormatTmp("mindseye_crash_%04u%02u%02u_%02u%02u%02u_%lu",
                        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, pid);
    }
    char reportBase[128] = {};
    StringCopy(StringView(reportBase, sizeof(reportBase)), crashFolderName);
    StringView reportBaseView = StringFromCString(reportBase);

    char dumpPath[ME_PATH_MAX] = {};
    StringCopy(StringView(dumpPath, ME_PATH_MAX), StringFormatTmp(STRING_FMT "\\" STRING_FMT ".dmp", STRING_VAARGS(reportsDirView), STRING_VAARGS(reportBaseView)));
    StringView dumpPathView = StringFromCString(dumpPath);
    char zipPath[ME_PATH_MAX] = {};
    StringCopy(StringView(zipPath, ME_PATH_MAX), StringFormatTmp(STRING_FMT "\\" STRING_FMT ".zip", STRING_VAARGS(reportsDirView), STRING_VAARGS(reportBaseView)));
    StringView zipPathView = StringFromCString(zipPath);

    EXCEPTION_RECORD* record = exceptionInfo->ExceptionRecord;
    StringBuilder stackTrace = CrashCreateStackTraceBuilder();
    stackTrace.Append(StringFormatTmp(
        "Unhandled exception 0x%08lx at 0x%p\nProcess: %lu\nThread: %lu\n\nStack trace:\n",
        record->ExceptionCode, record->ExceptionAddress, pid, GetCurrentThreadId()));
    CrashAppendStackTrace(exceptionInfo, stackTrace);
    StringView stackTraceText = StringView(stackTrace);

    // TODO: copy the log file into the zip as well
    bool wroteDump = CrashWriteDump(exceptionInfo, dumpPathView, crashContext);

    StringView zipEntries[2] = {};
    u32 zipEntryCount = 0;
    if (wroteDump)
    {
        zipEntries[zipEntryCount++] = dumpPathView;
    }

    bool wroteZip = ME_CRASHHANDLER_AUTO_ZIP ? CrashCreateZip(zipPathView, zipEntries, zipEntryCount) : false;
    if (wroteZip)
    {
        LOG_INFO("Mindseye wrote .zip to " STRING_FMT, STRING_VAARGS(zipPathView));
    }
    StringView dumpReportPath = wroteDump ? dumpPathView : STRING_LIT("(failed to write dump)");

    LOG_ERROR("\n%.*s", STRING_VAARGS(stackTraceText));
    LOG_ERROR("Crash report written: dump=" STRING_FMT,
        STRING_VAARGS(dumpReportPath));

    bool shouldDisplayMsgBox = !crashContext.isRunningTests && !isDebugging;
    if (shouldDisplayMsgBox)
    {
        char dialogText[2048] = {};
        StringCopy(StringView(dialogText, sizeof(dialogText)), StringFormatTmp(
            "Mindseye has crashed.\n\nDump:\n" STRING_FMT "\n\nThe stack trace was also printed to the debugger/console.",
            STRING_VAARGS(dumpReportPath)));
        MessageBoxA(nullptr, dialogText, "Mindseye Crash", MB_OK | MB_ICONERROR | MB_TASKMODAL);
    }

    if (isDebugging)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    ExitProcess(record->ExceptionCode);
    return EXCEPTION_EXECUTE_HANDLER;
}

void meOSInstallUnhandledExceptionHandler(meCrashHandlerContext context)
{
    g_crashHandlerContext = context;
    SetUnhandledExceptionFilter(meUnhandledExceptionFilter);
}
#endif

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
		case WM_KILLFOCUS:
		{
			osState->keyboardState.keyStates.clear();
			osState->keyboardState.prevKeyStates.clear();
			osState->mouseState.buttons = 0;
			osState->mouseState.prevButtons = 0;
		} break;
		case WM_CAPTURECHANGED:
		{
			// Windows has forcibly released our mouse capture
			// If we were in CAPTURED mode we need to clean up
			if (osState->useRawInput)
			{
				meOSSetCursorState(FREE, *osState);
			}
		} break;
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
        { osState->mouseState.scroll += (int)(short)HIWORD(wParam) / WHEEL_DELTA; break; } // normalize to ±1 per notch
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}


void meOSCreateWindow(WindowCreationParams creationParams, EngineContext* engine)
{
    ME_PROFILE_FUNCTION();
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
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));

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

    engine->osData = &g_osData;
    OSStateView* osData = engine->osData;
    osData->hinstance = hInstance;
    osData->hwnd = hwnd;
    osData->windowWidth = creationParams.width;
    osData->windowHeight = creationParams.height;
	QueryPerformanceFrequency((LARGE_INTEGER *)&osData->ticksPerSecond);
	QueryPerformanceCounter((LARGE_INTEGER *)&osData->ticksAtAppStart);
    engine->appName = creationParams.name;
}

void meOSTick(EngineContext* engine)
{
    ME_PROFILE_FUNCTION();
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
    ME_PROFILE_FUNCTION();
    return (void*)LoadLibraryA(name);
}

void UnloadDynamicLibrary(void* module)
{
    FreeLibrary((HMODULE)module);
}

void* GetFunctionPtr(void* module, StringView functionName)
{
    return (void*)GetProcAddress((HMODULE)module, functionName.cstr());
}

static volatile LONG* meWinAtomicPtr(meAtomicU32& atomic)
{
    return reinterpret_cast<volatile LONG*>(&atomic.value);
}

static volatile LONG* meWinAtomicPtr(const meAtomicU32& atomic)
{
    return const_cast<volatile LONG*>(reinterpret_cast<const volatile LONG*>(&atomic.value));
}

static volatile LONG64* meWinAtomicPtr(meAtomicU64& atomic)
{
    return reinterpret_cast<volatile LONG64*>(&atomic.value);
}

static volatile LONG64* meWinAtomicPtr(const meAtomicU64& atomic)
{
    return const_cast<volatile LONG64*>(reinterpret_cast<const volatile LONG64*>(&atomic.value));
}

u32 meOSAtomicLoad(const meAtomicU32& atomic)
{
    return (u32)InterlockedCompareExchange(meWinAtomicPtr(atomic), 0, 0);
}

u64 meOSAtomicLoad(const meAtomicU64& atomic)
{
    return (u64)InterlockedCompareExchange64(meWinAtomicPtr(atomic), 0, 0);
}

void meOSAtomicStore(meAtomicU32& atomic, u32 value)
{
    InterlockedExchange(meWinAtomicPtr(atomic), (LONG)value);
}

void meOSAtomicStore(meAtomicU64& atomic, u64 value)
{
    InterlockedExchange64(meWinAtomicPtr(atomic), (LONG64)value);
}

u32 meOSAtomicExchange(meAtomicU32& atomic, u32 value)
{
    return (u32)InterlockedExchange(meWinAtomicPtr(atomic), (LONG)value);
}

u64 meOSAtomicExchange(meAtomicU64& atomic, u64 value)
{
    return (u64)InterlockedExchange64(meWinAtomicPtr(atomic), (LONG64)value);
}

u32 meOSAtomicCompareExchange(meAtomicU32& atomic, u32 exchange, u32 comparand)
{
    return (u32)InterlockedCompareExchange(meWinAtomicPtr(atomic), (LONG)exchange, (LONG)comparand);
}

u64 meOSAtomicCompareExchange(meAtomicU64& atomic, u64 exchange, u64 comparand)
{
    return (u64)InterlockedCompareExchange64(meWinAtomicPtr(atomic), (LONG64)exchange, (LONG64)comparand);
}

u32 meOSAtomicAdd(meAtomicU32& atomic, u32 value)
{
    return (u32)InterlockedExchangeAdd(meWinAtomicPtr(atomic), (LONG)value);
}

u64 meOSAtomicAdd(meAtomicU64& atomic, u64 value)
{
    return (u64)InterlockedExchangeAdd64(meWinAtomicPtr(atomic), (LONG64)value);
}

u32 meOSAtomicIncrement(meAtomicU32& atomic)
{
    return (u32)InterlockedIncrement(meWinAtomicPtr(atomic));
}

u64 meOSAtomicIncrement(meAtomicU64& atomic)
{
    return (u64)InterlockedIncrement64(meWinAtomicPtr(atomic));
}

u32 meOSAtomicDecrement(meAtomicU32& atomic)
{
    return (u32)InterlockedDecrement(meWinAtomicPtr(atomic));
}

u64 meOSAtomicDecrement(meAtomicU64& atomic)
{
    return (u64)InterlockedDecrement64(meWinAtomicPtr(atomic));
}

bool meOSMapFile(meMemoryMappedFile& out, StringView path, OSFileFlags flags)
{
    ME_PROFILE_FUNCTION();
    ME_ASSERT(!out.ptr);
    ME_ASSERT(path);
    if (!meOSOpenFile(out, path, flags))
    {
        LOG_ERROR("meOSMapFile: failed to open " STRING_FMT, STRING_VAARGS(path));
        return false;
    }
    out.size = meOSGetFileSize(out);
    if (out.size == 0)
    {
        meOSUnmapFile(out);
        return false;
    }

    DWORD protect = (flags & OSFileFlags_ReadOnly) ? PAGE_READONLY : PAGE_READWRITE;
    HANDLE mapping = CreateFileMappingA(out.fileHandle, nullptr, protect, 0, 0, nullptr);
    if (!mapping)
    {
        LOG_ERROR("meOSMapFile: CreateFileMappingA failed for " STRING_FMT " (%lu)", STRING_VAARGS(path), GetLastError());
        meOSUnmapFile(out);
        return false;
    }

    DWORD desiredAccess = (flags & OSFileFlags_ReadOnly) ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS;
    void* ptr = MapViewOfFile(mapping, desiredAccess, 0, 0, 0);
    if (!ptr)
    {
        LOG_ERROR("meOSMapFile: MapViewOfFile failed for " STRING_FMT " (%lu)", STRING_VAARGS(path), GetLastError());
        CloseHandle(mapping);
        meOSUnmapFile(out);
        return false;
    }

    out.mappingHandle = mapping;
    out.ptr           = ptr;
    return true;
}

void meOSUnmapFile(meMemoryMappedFile& mapping)
{
    ME_PROFILE_FUNCTION();
    if (mapping.ptr)            UnmapViewOfFile(mapping.ptr);
    if (mapping.mappingHandle)  CloseHandle((HANDLE)mapping.mappingHandle);
    if (mapping.HasOpenFile())  meOSCloseFile(mapping);
    mapping = {};
}

void meOSCommitMemory(void* ptr, u64 size)
{
    VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
}

void* meOSAllocWriteTracked(u64 size)
{
    return VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE);
}

void meOSGetWrittenAddresses(void* base, u64 size, void** pagesOut, u64* countInOut, u64* pageSize)
{
    ULONG_PTR count = (ULONG_PTR)*countInOut;
    ULONG ps = 0;
    GetWriteWatch(0, base, (SIZE_T)size, pagesOut, &count, &ps);
    *countInOut = (u64)count;
    *pageSize   = (u64)ps;
}

void meOSResetWrittenAddresses(void* base, u64 size)
{
    ResetWriteWatch(base, (SIZE_T)size);
}

struct meProcessWatchData
{
    HANDLE                waitHandle;
    HANDLE                processHandle;
    meProcessExitCallback callback;
    void*                 userData;
};

static VOID CALLBACK ProcessExitCallback(PVOID param, BOOLEAN)
{
    auto* wd = (meProcessWatchData*)param;
    UnregisterWaitEx(wd->waitHandle, nullptr);
    DWORD exitCode = 1;
    GetExitCodeProcess(wd->processHandle, &exitCode);
    CloseHandle(wd->processHandle);
    wd->callback((s32)exitCode, wd->userData);
    delete wd;
}

void* meOSRunProcessAsync(const char* workingDir, StringView command, meProcessExitCallback onExit, void* userData)
{
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    ME_ASSERT(command.len < 4096);
    char cmdBuf[4096];
    ME_MEMCPY(cmdBuf, command.data, command.len);
    cmdBuf[command.len] = '\0';

    BOOL ok = CreateProcessA(nullptr, cmdBuf, nullptr, nullptr, FALSE, 0, nullptr, workingDir, &si, &pi);
    if (!ok)
    {
        LOG_ERROR("[meOS] Failed to spawn process (err=%lu)", GetLastError());
        return nullptr;
    }

    CloseHandle(pi.hThread);

    if (onExit)
    {
        auto* wd = new meProcessWatchData{ nullptr, pi.hProcess, onExit, userData };
        RegisterWaitForSingleObject(&wd->waitHandle, pi.hProcess, ProcessExitCallback, wd, INFINITE, WT_EXECUTEONLYONCE);
        return nullptr;
    }

    return (void*)pi.hProcess;
}

s32 meOSWaitForProcess(void* processHandle)
{
    ME_PROFILE_FUNCTION();
    HANDLE h = (HANDLE)processHandle;
    WaitForSingleObject(h, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(h, &exitCode);
    CloseHandle(h);
    return (s32)exitCode;
}


bool meOSReadFileContents(const OSFileReference& file, void* backingBuffer, size_t backingBufferSize)
{
    ME_PROFILE_FUNCTION();
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
	const void* buffer,
	size_t amtToWrite)
{
    ME_PROFILE_FUNCTION();
    ME_ASSERT(!(file.flags & OSFileFlags_ReadOnly));
	const u8* data = (const u8*)buffer;
	size_t totalWritten = 0;
	while (totalWritten < amtToWrite)
	{
		DWORD thisWrite = (DWORD)MEMIN(amtToWrite - totalWritten, (size_t)0xffffffffu);
		DWORD amtActuallyWritten = 0;
		bool result = WriteFile(file.fileHandle, data + totalWritten, thisWrite, &amtActuallyWritten, nullptr);
		if (!result || amtActuallyWritten == 0)
		{
			DWORD err = GetLastError();
			LOG_ERROR("[meOS] failed to write file contents %s err code = %u", file.path, err);
			return false;
		}
		totalWritten += amtActuallyWritten;
	}
	return true;
}

bool meOSWriteFileContentAtomic(
	StringView path,
	const void* buffer,
	size_t amtToWrite)
{
	ME_PROFILE_FUNCTION();
	ME_ASSERT(path);

	FILETIME timestampFiletime = {};
	GetSystemTimePreciseAsFileTime(&timestampFiletime);
	u64 timestamp = ((u64)timestampFiletime.dwHighDateTime << 32) | (u64)timestampFiletime.dwLowDateTime;

	StringView tempPath = StringFormatTmp(STRING_FMT ".tmp.%llu", STRING_VAARGS(path), timestamp);
	if (tempPath.len >= ME_PATH_MAX)
	{
		LOG_ERROR("[meOS] failed atomic write because temp path is too long: " STRING_FMT, STRING_VAARGS(path));
		return false;
	}

	OSFileReference tempFile = {};
	if (!meOSOpenFile(tempFile, tempPath, OSFileFlags_StompExisting | OSFileFlags_Temporary | OSFileFlags_DeleteOnFileClose | OSFileFlags_ScopedFile))
	{
		LOG_ERROR("[meOS] failed atomic write because temp file could not be opened: " STRING_FMT, STRING_VAARGS(tempPath));
		return false;
	}

	bool success = meOSWriteFileContent(tempFile, buffer, amtToWrite);
	if (!success)
	{
		return false;
	}

	OSFileReference targetFile = {};
	targetFile.InitWithoutOpening(path);
	if (!meOSFileMove(tempFile, targetFile))
	{
		LOG_ERROR("[meOS] failed atomic rename " STRING_FMT " -> " STRING_FMT,
			STRING_VAARGS(tempPath),
			STRING_VAARGS(path));
		return false;
	}

	return true;
}

u64 meOSGetFileSize(const OSFileReference& file)
{
    ME_PROFILE_FUNCTION();
    LARGE_INTEGER fileSize;
    bool result = false;
    if (file.HasOpenFile())
    {
        result = GetFileSizeEx(file.fileHandle, &fileSize);
    }
    else
    {
        WIN32_FILE_ATTRIBUTE_DATA fileData = {};
        result = GetFileAttributesExA(file.path, GetFileExInfoStandard, &fileData);
        fileSize.HighPart = fileData.nFileSizeHigh;
        fileSize.LowPart = fileData.nFileSizeLow;
    }
    if (!result)
    {
        LOG_WARN("Failed to get file size %s", file.path);
        return 0;
    }
    return fileSize.QuadPart;
}


MEAPI FileTimestamps meOSGetFileTimestamps(
	const OSFileReference& file)
{
    ME_PROFILE_FUNCTION();
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
    ME_PROFILE_FUNCTION();
    ME_MEMCLEAR((void*)file.path, ME_PATH_MAX);
    StringCopy({file.path, ME_PATH_MAX}, path);
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
    file.fileHandle = CreateFileA(file.path, (flags & OSFileFlags_ReadOnly) ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, openMode, 
                        FILE_ATTRIBUTE_NORMAL | 
                        (flags & OSFileFlags_Temporary ? FILE_ATTRIBUTE_TEMPORARY : 0) |
                        (flags & OSFileFlags_DeleteOnFileClose ? FILE_FLAG_DELETE_ON_CLOSE : 0)
                        , 0);
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
    ME_PROFILE_FUNCTION();
    ME_ASSERT(file.fileHandle != nullptr && file.fileHandle != INVALID_HANDLE_VALUE);
	bool result = CloseHandle(file.fileHandle);
	file.fileHandle = nullptr;
    return result;
}

bool meOSDeleteFile(
	OSFileReference& file)
{
    ME_PROFILE_FUNCTION();
	bool result = DeleteFileA(file.path);
	return result;
}

bool meOSFileCopy(
	const OSFileReference& src,
	const OSFileReference& dst,
	bool failIfExists)
{
	ME_PROFILE_FUNCTION();
	bool result = CopyFileA(src.path, dst.path, failIfExists) != 0;
	if (!result)
	{
		DWORD error = GetLastError();
		LOG_ERROR("[meOS] failed to copy file %s -> %s err code = %u", src.path, dst.path, error);
	}
	return result;
}

bool meOSFileMove(
	const OSFileReference& src,
	const OSFileReference& dst,
	bool failIfExists)
{
	ME_PROFILE_FUNCTION();
	DWORD moveFlags = 0;
	if (!failIfExists)
	{
		moveFlags |= MOVEFILE_REPLACE_EXISTING;
	}
	bool result = MoveFileExA(src.path, dst.path, moveFlags) != 0;
	if (!result)
	{
		DWORD error = GetLastError();
		LOG_ERROR("[meOS] failed to move file %s -> %s err code = %u", src.path, dst.path, error);
	}
	return result;
}

bool meOSReadDirectory(
	const OSFileReference& folder,
	DynArray<OSFileReference>& result)
{
    ME_PROFILE_FUNCTION();
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
	static char path[ME_PATH_MAX];
	if (path[0] == '\0')
	{
		ME_MEMCLEAR(path, ME_PATH_MAX);
		GetModuleFileNameA(NULL, path, sizeof(path));
		meFsNormalizePathSeperators(StringView(path, CStringLength(path)));
	}
	return StringView(path, CStringLength(path));
}

StringView meOSGetExeFileFolder()
{
	static char path[ME_PATH_MAX];
	if (path[0] == '\0')
	{
		ME_MEMCLEAR(path, ME_PATH_MAX);
		StringView fullPath = meOSGetExeFilepath();
		s32 lastDirSep = FindInStringRev(fullPath, meFsGetDirectorySeperator());
		ME_MEMCPY(path, fullPath.cstr(), lastDirSep);
	}
	return StringView(path, CStringLength(path));
}

StringView meOSGetWorkingDir()
{
	static char workingDirBuffer[ME_PATH_MAX];
	if (workingDirBuffer[0] == '\0')
	{
		ME_MEMCLEAR(workingDirBuffer, ME_PATH_MAX);
		DWORD dwRet = GetCurrentDirectoryA(ME_PATH_MAX, workingDirBuffer);
		if (dwRet == 0) 
		{
			// failure
			DWORD error = GetLastError();
			LOG_ERROR("Failed to get working directory. Err %i", error);
			return {};
		} 
		else if (dwRet > ME_PATH_MAX) 
		{
			// path is too long for the buffer
			ME_ASSERT(false && "working dir is more than ME_PATH_MAX characters");
		}
		StringView result = StringFromCString(workingDirBuffer, ME_PATH_MAX);
		meFsNormalizePathSeperators(result);
		return result;
	}
	StringView result = StringFromCString(workingDirBuffer, ME_PATH_MAX);
	return result;
}

BOOL DirectoryExists(const char* dirPath) 
{
    ME_PROFILE_FUNCTION();
    DWORD fileAttributes = GetFileAttributesA(dirPath);
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        return FALSE;
    }
    return (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// Function to create a full directory path recursively
bool CreateRecursiveDirectory(StringView path) 
{
    ME_PROFILE_FUNCTION();
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
    if (p[0] && p[1] == ':' && p[2] == '\\') 
    {
        p += 3;
    }
    
    while (*p) 
    {
        if (*p == '\\' || *p == '/') 
        {
            *p = '\0'; // Temporarily terminate the string
            
            if (!DirectoryExists(tempPath)) 
            {
                if (!CreateDirectoryA(tempPath, NULL)) 
                {
                    DWORD error = GetLastError();
                    if (error != ERROR_ALREADY_EXISTS) 
                    {
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
    if (!DirectoryExists(tempPath)) 
    {
        if (!CreateDirectoryA(tempPath, NULL)) 
        {
            DWORD error = GetLastError();
            if (error != ERROR_ALREADY_EXISTS) 
            {
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
    ME_PROFILE_FUNCTION();
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
    ME_PROFILE_FUNCTION();
    char dst[ME_PATH_MAX];
    DWORD result = GetFullPathNameA(potentiallyRelativePath.cstr(), ME_PATH_MAX, dst, NULL);
	ME_ASSERT(result > 0);
	u64 len = CStringLength(dst);
	String absolutePath = String((const char*)dst, len, allocator);
	meFsNormalizePathSeperators(absolutePath);
	return absolutePath;
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
