#pragma once

#include "core/me_defines.h"
#include "core/me_math.h"

// win32
#define KEY_PRESSED 0x8000

enum meMouseButton
{
	LBUTTON, RBUTTON, MBUTTON,
};

enum meOSCursorState
{
	// OS visible cursor, can click and navigate with mouse normally
	FREE, 
	// cursor hidden and locked into window
	CAPTURED,
};

struct meMouseInput 
{
    glm::vec2 mousePosScreen = glm::vec2(0.0f, 0.0f);
	glm::vec2 mouseDelta = glm::vec2(0.0f, 0.0f);
	s32 scroll = 0;
	u32 buttons = 0;

    MEAPI bool isMouseButtonDown(s32 button);
    MEAPI bool isMouseButtonUp(s32 button);
    MEAPI void UpdateMouseScreenPos(f32 xpos, f32 ypos);
};