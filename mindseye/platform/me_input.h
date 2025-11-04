#pragma once

#include "core/me_defines.h"
#include "core/me_math.h"
#include "core/containers/me_bitarray.h"

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
	u32 prevButtons = 0;
	u32 buttons = 0;

    MEAPI bool IsMouseButtonDown(s32 meMouseButton) const;
	MEAPI bool IsMouseButtonJustPressed(s32 meMouseButton) const;
	MEAPI bool IsMouseButtonJustReleased(s32 meMouseButton) const;

    MEAPI void UpdateMouseScreenPos(f32 xpos, f32 ypos);
	MEAPI void MouseInputTick();
};

enum meVirtualKey
{
	MEKEY_NONE = 0,
	// at the moment, the "virtual keys" passed to the input funcs
	// are the win32 virtual key codes.
	// in the future, we might want to map an OS's keycodes to our own
};

struct meKeyboardInput
{
	// there are 254 virtual key codes in win32
	meBitArray<256> keyStates = {};
	meBitArray<256> prevKeyStates = {};

	MEAPI bool IsKeyDown(s32 meVirtualKey) const;
	MEAPI bool IsKeyJustPressed(s32 meVirtualKey) const;
	MEAPI bool IsKeyJustReleased(s32 meVirtualKey) const;

	MEAPI void KeyboardInputTick();
};