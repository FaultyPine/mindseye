#include "me_input.h"


void meMouseInput::UpdateMouseScreenPos(f32 xpos, f32 ypos) 
{
    mousePosScreen = glm::vec2(xpos, ypos);
}

bool meMouseInput::IsMouseButtonDown(s32 button) const
{
	return TEST_BIT(buttons, button);
}

bool meMouseInput::IsMouseButtonJustPressed(s32 meMouseButton) const
{
	return IsMouseButtonDown(meMouseButton) && !TEST_BIT(prevButtons, meMouseButton);
}

bool meMouseInput::IsMouseButtonJustReleased(s32 meMouseButton) const
{
	return !IsMouseButtonDown(meMouseButton) && TEST_BIT(prevButtons, meMouseButton);
}

void meMouseInput::MouseInputTick()
{
	prevButtons = buttons;
	buttons = 0;
}

void meKeyboardInput::KeyboardInputTick()
{
	prevKeyStates = keyStates;
}

bool meKeyboardInput::IsKeyDown(s32 meVirtualKey) const
{
	return keyStates.get(meVirtualKey);
}

bool meKeyboardInput::IsKeyJustPressed(s32 meVirtualKey) const
{
	return IsKeyDown(meVirtualKey) && !prevKeyStates.get(meVirtualKey);
}

bool meKeyboardInput::IsKeyJustReleased(s32 meVirtualKey) const
{
	return !IsKeyDown(meVirtualKey) && prevKeyStates.get(meVirtualKey);
}