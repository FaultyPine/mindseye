#pragma once

#if defined(TRACY_ENABLE)
#include "tracy/Tracy.hpp"

#define ME_PROFILE_SCOPE(name) ZoneScopedN(name)
#define ME_PROFILE_FUNCTION() ZoneScoped
#define ME_PROFILE_FRAME_MARKER() FrameMark
#else
#define ME_PROFILE_SCOPE(name) ((void)0)
#define ME_PROFILE_FUNCTION() ((void)0)
#define ME_PROFILE_FRAME_MARKER() ((void)0)
#endif
