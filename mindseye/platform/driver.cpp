

#include "mindseye/core/me_defines.h"
#include "mindseye/core/me_log.h"
#include "mindseye/platform/me_os.h"


#ifdef OS_WINDOWS

#include <stdlib.h> // for the argc and argv macros... would rather not have to pull this whole thing in
#include <windows.h>

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    return meOSMain(__argc, __argv);
}
#endif