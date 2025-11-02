#include "me_input.h"


void meMouseInput::UpdateMouseScreenPos(f32 xpos, f32 ypos) 
{
    mousePosScreen = glm::vec2(xpos, ypos);
}

bool meMouseInput::isMouseButtonDown(s32 button)
{
	UNIMPLEMENTED();
}
bool meMouseInput::isMouseButtonUp(s32 button)
{
	UNIMPLEMENTED();
}