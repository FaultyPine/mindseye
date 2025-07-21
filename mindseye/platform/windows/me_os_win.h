#pragma once

#include "core/me_defines.h"
#include "platform/me_os.h"

#ifdef OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif



#ifdef OS_WINDOWS
MEAPI int me_os_win_main(int argc, char** argv);
MEAPI int me_os_win_main(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow);

// int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
// {
//     return me_os_win_main(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
// }
#endif




