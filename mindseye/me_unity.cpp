
#define UNITY_BUILD

// HEADER
#ifdef OS_WINDOWS
#include "me_os_win.h"
#endif
#include "core/me_log.h"
#include "core/me_string.h"

// SOURCE
#ifdef OS_WINDOWS
#include "me_os_win.cpp"
#endif
#include "core/me_log.cpp"
#include "core/me_string.cpp"


