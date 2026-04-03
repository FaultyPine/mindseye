// Precompiled header for mindseye
// Contains STL, OS, and 3rd-party headers already used by the project.

#pragma once

#include "core/me_defines.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

#ifdef OS_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#endif

#include <external/glm/glm.hpp>
#include <external/glm/gtc/matrix_transform.hpp>
#include <external/glm/gtc/type_ptr.hpp>
#include <external/glm/gtx/quaternion.hpp>
#include <external/glm/gtx/matrix_decompose.hpp>
#include <external/glm/gtx/string_cast.hpp>

#include <external/json.hpp>

#include "external/concurrentqueue/concurrentqueue.h"

#include "external/bgfx/bx/include/bx/bx.h"
#include "external/bgfx/bgfx/include/bgfx/bgfx.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_API MEAPI
#include "external/bgfx/bgfx/3rdparty/dear-imgui/imgui.h"
