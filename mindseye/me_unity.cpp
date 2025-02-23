
#define UNITY_BUILD

// third party
// TODO: pch
#define STB_SPRINTF_IMPLEMENTATION
#include "external/stb_sprintf.h"
#undef STB_SPRINTF_IMPLEMENTATION
#include "external/result.h"

// HEADER
#ifdef OS_WINDOWS
#include "me_os_win.h"
#endif
#include "core/me_log.h"
#include "core/me_string.h"
#include "core/me_arena.h"

// SOURCE
#ifdef OS_WINDOWS
#include "me_os_win.cpp"
#endif
#include "core/me_log.cpp"
#include "core/me_string.cpp"
#include "core/me_arena.cpp"

