
#define UNITY_BUILD

// third party
// TODO: pch
#define STB_SPRINTF_IMPLEMENTATION
#include "external/stb_sprintf.h"
#undef STB_SPRINTF_IMPLEMENTATION
#include "external/result.h"

// HEADER
#include "platform/me_os.h"
#include "core/me_log.h"
#include "core/me_string.h"
#include "core/me_arena.h"
#include "core/me_math.h"
#include "core/me_core.h"

// SOURCE
#include "platform/me_os.cpp"
#include "core/me_log.cpp"
#include "core/me_string.cpp"
#include "core/me_arena.cpp"
#include "core/me_math.cpp"
#include "core/me_core.cpp"

